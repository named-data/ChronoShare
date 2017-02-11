/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#include "chronosharegui.hpp"
#include "core/logging.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QValidator>

#include <boost/throw_exception.hpp>
#include <thread>

namespace ndn {
namespace chronoshare {

namespace fs = boost::filesystem;

static const std::string HTTP_SERVER_ADDRESS = "localhost";
static const std::string HTTP_SERVER_PORT = "9001";
#ifdef _DEBUG
static const std::string DOC_ROOT = "gui/html";
#else
static const std::string DOC_ROOT = ":/html";
#endif
static const QString ICON_BIG_FILE(":/images/chronoshare-big.png");
static const QString ICON_TRAY_FILE(":/images/" TRAY_ICON);

_LOG_INIT(Gui);

ChronoShareGui::ChronoShareGui(QWidget* parent)
  : QDialog(parent)
  , m_httpServer(0)
#ifdef AUTOUPDATE
  , m_sparkle(CHRONOSHARE_APPCAST)
#endif
{
  setWindowTitle("Settings");
  setMinimumWidth(600);

  labelUsername = new QLabel("Username(hint: /<username>)");
  labelSharedFolder = new QLabel("Shared Folder Name");
  labelSharedFolderPath = new QLabel("Shared Folder Path");

  QRegExp regex("(/[^/]+)+$");
  QValidator* prefixValidator = new QRegExpValidator(regex, this);

  editUsername = new QLineEdit();
  editUsername->setValidator(prefixValidator);

  QRegExp noPureWhiteSpace("^\\S+.*$");
  QValidator* wsValidator = new QRegExpValidator(noPureWhiteSpace, this);
  editSharedFolder = new QLineEdit();
  editSharedFolder->setValidator(wsValidator);

  editSharedFolderPath = new QLineEdit();
  editSharedFolderPath->setReadOnly(true);
  QPalette pal = editSharedFolderPath->palette();
  pal.setColor(QPalette::Active, QPalette::Base, pal.color(QPalette::Disabled, QPalette::Base));
  editSharedFolderPath->setPalette(pal);
  button = new QPushButton("Save and apply settings");

  QString versionString = QString("Version: ChronoShare v%1").arg(CHRONOSHARE_VERSION);
  label = new QLabel(versionString, this);

  connect(button, SIGNAL(clicked()), this, SLOT(changeSettings()));

  mainLayout = new QVBoxLayout; // vertically
  mainLayout->addWidget(labelUsername);
  mainLayout->addWidget(editUsername);
  mainLayout->addWidget(labelSharedFolder);
  mainLayout->addWidget(editSharedFolder);
  mainLayout->addWidget(labelSharedFolderPath);
  mainLayout->addWidget(editSharedFolderPath);
  mainLayout->addWidget(button);
  mainLayout->addWidget(label);
  setLayout(mainLayout);

  // create actions that result from clicking a menu option
  createActionsAndMenu();

  // create tray icon
  createTrayIcon();

  // set icon image
  setIcon();

  // show tray icon
  m_trayIcon->show();

  // load settings
  if (!loadSettings() || m_username.isNull() || m_username == "" || m_sharedFolderName.isNull() ||
      m_sharedFolderName == "" || m_dirPath.isNull() || m_dirPath == "") {
    // prompt user to choose folder
    _LOG_DEBUG("First Time Setup");
    openMessageBox("First Time Setup",
                   "Please enter a username, shared folder name and choose the shared folder path on your local filesystem.");
    viewSettings();
    openFileDialog();
    viewSettings();
  }
  else {
    startBackend();
  }
}

ChronoShareGui::ChronoShareGui(QString dirPath, QString username, QString sharedFolderName, QWidget* parent)
  : QDialog(parent)
  , m_dirPath(dirPath)
  , m_username(username)
  , m_sharedFolderName(sharedFolderName)
#ifdef AUTOUPDATE
  , m_sparkle(CHRONOSHARE_APPCAST)
#endif
{
  if (m_username.isNull() || m_username == "" || m_sharedFolderName.isNull() ||
      m_sharedFolderName == "" || m_dirPath.isNull() || m_dirPath == "") {
    // prompt user to choose folder
    BOOST_THROW_EXCEPTION(Error("Some error with init info"));
  }
  else {
    startBackend();
  }
}

void
ChronoShareGui::startBackend(bool restart /*=false*/)
{
  if (m_username.isNull() || m_username == "" || m_sharedFolderName.isNull() ||
      m_sharedFolderName == "" || m_dirPath.isNull() || m_dirPath == "") {
    _LOG_DEBUG("Don't start backend, because settings are in progress or incomplete");
    return;
  }

  if (m_watcher != 0 && m_dispatcher != 0) {
    if (!restart) {
      return;
    }

    _LOG_DEBUG("Restarting Dispatcher and FileWatcher for the new location or new username");
    m_watcher.reset();    // stop filewatching ASAP
    m_dispatcher.reset(); // stop dispatcher ASAP, but after watcher(to prevent triggering callbacks
                          // on deleted object)
    m_ioServiceWork.reset();
    m_ioServiceManager->handle_stop();
    m_NetworkThread.join();
    delete m_ioServiceManager;
  }

  fs::path realPathToFolder(m_dirPath.toStdString());
  realPathToFolder /= m_sharedFolderName.toStdString();

  _LOG_DEBUG("m_dispatcher username:" << m_username.toStdString()
            << " m_sharedFolderName:" << m_sharedFolderName.toStdString()
            << " realPathToFolder: " << realPathToFolder);

  m_ioService.reset(new boost::asio::io_service());
  m_face.reset(new Face(*m_ioService));
  m_dispatcher.reset(new Dispatcher(m_username.toStdString(), m_sharedFolderName.toStdString(),
                                    realPathToFolder, *m_face));

  // Alex: this **must** be here, otherwise m_dirPath will be uninitialized
  m_watcher.reset(new FsWatcher(*m_ioService, realPathToFolder.string().c_str(),
                                bind(&Dispatcher::Did_LocalFile_AddOrModify, m_dispatcher.get(), _1),
                                bind(&Dispatcher::Did_LocalFile_Delete, m_dispatcher.get(), _1)));
  try {
    m_ioServiceManager = new IoServiceManager(*m_ioService);
    m_NetworkThread = std::thread(&IoServiceManager::run, m_ioServiceManager);
  }
  catch (const std::exception& e) {
    _LOG_ERROR("Start IO service or Face failed");
    openWarningMessageBox("", "WARNING: Cannot allocate thread for face and io_service!",
                          QString("Starting chronoshare failed"
                                  "Exception caused: %1")
                            .arg(e.what()));
    // stop filewatching ASAP
    m_watcher.reset();
    m_dispatcher.reset();
    return;
  }

  if (m_httpServer != 0) {
    // no need to restart webserver if it already exists
    return;
  }

  QFileInfo indexHtmlInfo(":/html/index.html");
  if (indexHtmlInfo.exists()) {
    try {
      m_httpServer = new http::server::server(HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT, DOC_ROOT);
      m_httpServerThread = std::thread(&http::server::server::run, m_httpServer);
    }
    catch (const std::exception& e) {
      _LOG_ERROR("Start http server failed");
      m_httpServer = 0; // just to make sure
      openWarningMessageBox("WARNING", "WARNING: Cannot start http server!",
                            QString("Starting http server failed. You will "
                                    "not be able to check history from web "
                                    "brower. Exception caused: %1")
                              .arg(e.what()));
    }
  }
  else {
    _LOG_ERROR("Http server doc root dir does not exist!");
  }
}

ChronoShareGui::~ChronoShareGui()
{
  // stop filewatching ASAP
  m_watcher.reset();

  // stop dispatcher ASAP, but after watcher to prevent triggering callbacks on the deleted object
  m_dispatcher.reset();

  m_ioServiceWork.reset();
  m_ioServiceManager->handle_stop();
  m_NetworkThread.join();
  delete m_ioServiceManager;

  if (m_httpServer != 0) {
    m_httpServer->handle_stop();
    m_httpServerThread.join();
    delete m_httpServer;
  }

  // cleanup
  delete m_trayIcon;
  delete m_trayIconMenu;

  delete m_openFolder;
  delete m_viewSettings;
  delete m_changeFolder;
  delete m_quitProgram;

  delete labelUsername;
  delete labelSharedFolder;
  delete editUsername;
  delete editSharedFolder;
  delete button;
  delete label;
  delete mainLayout;
}

void
ChronoShareGui::openMessageBox(QString title, QString text)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);

  messageBox.setIconPixmap(QPixmap(ICON_BIG_FILE));

  messageBox.exec();
}

void
ChronoShareGui::openMessageBox(QString title, QString text, QString infotext)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);
  messageBox.setInformativeText(infotext);

  messageBox.setIconPixmap(QPixmap(ICON_BIG_FILE));

  messageBox.exec();
}

void
ChronoShareGui::openWarningMessageBox(QString title, QString text, QString infotext)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);
  messageBox.setInformativeText(infotext);

  messageBox.setIcon(QMessageBox::Warning);
  messageBox.setStandardButtons(QMessageBox::Ok);

  messageBox.exec();
}

void
ChronoShareGui::createActionsAndMenu()
{
  _LOG_DEBUG("Create actions");

  // create the "open folder" action
  m_openFolder = new QAction(tr("&Open Folder"), this);
  connect(m_openFolder, SIGNAL(triggered()), this, SLOT(openSharedFolder()));

  m_openWeb = new QAction(tr("Open in &Browser"), this);
  connect(m_openWeb, SIGNAL(triggered()), this, SLOT(openInWebBrowser()));

  m_recentFilesMenu = new QMenu(tr("Recently Changed Files"), this);
  for (int i = 0; i < 5; i++) {
    m_fileActions[i] = new QAction(this);
    m_fileActions[i]->setVisible(false);
    connect(m_fileActions[i], SIGNAL(triggered()), this, SLOT(openFile()));
    m_recentFilesMenu->addAction(m_fileActions[i]);
  }
  connect(m_recentFilesMenu, SIGNAL(aboutToShow()), this, SLOT(updateRecentFilesMenu()));

  // create the "view settings" action
  m_viewSettings = new QAction(tr("&View Settings"), this);
  connect(m_viewSettings, SIGNAL(triggered()), this, SLOT(viewSettings()));

  // create the "change folder" action
  m_changeFolder = new QAction(tr("&Change Folder"), this);
  connect(m_changeFolder, SIGNAL(triggered()), this, SLOT(openFileDialog()));

#ifdef AUTOUPDATE
  m_checkForUpdates = new QAction("Check for updates", this);
  connect(m_checkForUpdates, SIGNAL(triggered()), this, SLOT(checkForUpdates()));
#endif // AUTOUPDATE

  // create the "quit program" action
  m_quitProgram = new QAction(tr("&Quit"), this);
  connect(m_quitProgram, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void
ChronoShareGui::createTrayIcon()
{
  // create a new icon menu
  m_trayIconMenu = new QMenu(this);

  // add actions to the menu
  m_trayIconMenu->addAction(m_openFolder);
  m_trayIconMenu->addAction(m_openWeb);
  m_trayIconMenu->addMenu(m_recentFilesMenu);
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_viewSettings);
  m_trayIconMenu->addAction(m_changeFolder);

#ifdef AUTOUPDATE
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_checkForUpdates);
#endif // AUTOUPDATE

  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_quitProgram);

  // create new tray icon
  m_trayIcon = new QSystemTrayIcon(this);

  // associate the menu with the tray icon
  m_trayIcon->setContextMenu(m_trayIconMenu);

  // handle left click of icon
  connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
          SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));
}

void
ChronoShareGui::setIcon()
{
  // set the icon image
  m_trayIcon->setIcon(QIcon(ICON_TRAY_FILE));
}

void
ChronoShareGui::openSharedFolder()
{
  fs::path realPathToFolder(m_dirPath.toStdString());
  realPathToFolder /= m_sharedFolderName.toStdString();
  QString path = QDir::toNativeSeparators(realPathToFolder.string().c_str());
  QDesktopServices::openUrl(QUrl("file:///" + path));
}

void
ChronoShareGui::openInWebBrowser()
{
  QUrl url("http://localhost:9001/");
  url.setFragment("folderHistory&"
                  "user=" +
                  QUrl::toPercentEncoding(m_username) + "&"
                                                        "folder=" +
                  QUrl::toPercentEncoding(m_sharedFolderName));

  // i give up. there is double encoding and I have no idea how to fight it...
  QDesktopServices::openUrl(url);
}

void
ChronoShareGui::openFile()
{
  // figure out who sent the signal
  QAction* pAction = qobject_cast<QAction*>(sender());
  if (pAction->isEnabled()) {
// we stored full path of the file in this toolTip field
#ifdef Q_WS_MAC
    // we do some hack so we could show the file in Finder highlighted
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + pAction->toolTip() + "/" + pAction->text() + "\"";
    args << "-e";
    args << "end tell";
    QProcess::startDetached("osascript", args);
#else
    // too bad qt couldn't do highlighting for Linux(or Mac)
    QDesktopServices::openUrl(QUrl("file:///" + pAction->toolTip()));
#endif
  }
}

void
ChronoShareGui::updateRecentFilesMenu()
{
  for (int i = 0; i < 5; i++) {
    m_fileActions[i]->setVisible(false);
  }
  m_dispatcher->LookupRecentFileActions(std::bind(&ChronoShareGui::checkFileAction, this, _1, _2, _3),
                                        5);
}

void
ChronoShareGui::checkFileAction(const std::string& filename, int action, int index)
{
  fs::path realPathToFolder(m_dirPath.toStdString());
  realPathToFolder /= m_sharedFolderName.toStdString();
  realPathToFolder /= filename;
  QString fullPath = realPathToFolder.string().c_str();
  QFileInfo fileInfo(fullPath);
  if (fileInfo.exists()) {
    // This is a hack, we just use some field to store the path
    m_fileActions[index]->setToolTip(fileInfo.absolutePath());
    m_fileActions[index]->setEnabled(true);
  }
  else {
    // after half an hour frustrating test and search around,
    // I think it's the problem of Qt.
    // According to the Qt doc, the action cannot be clicked
    // and the area would be grey, but it didn't happen
    // User can still trigger the action, and not greyed
    // added check in SLOT to see if the action is "enalbed"
    // as a remedy
    // Give up at least for now
    m_fileActions[index]->setEnabled(false);
    // UPDATE, file not fetched yet
    if (action == 0) {
      QFont font;
      // supposed by change the font, didn't happen
      font.setWeight(QFont::Light);
      m_fileActions[index]->setFont(font);
      m_fileActions[index]->setToolTip(tr("Fetching..."));
    }
    // DELETE
    else {
      QFont font;
      // supposed by change the font, didn't happen
      font.setStrikeOut(true);
      m_fileActions[index]->setFont(font);
      m_fileActions[index]->setToolTip(tr("Deleted..."));
    }
  }
  m_fileActions[index]->setText(fileInfo.fileName());
  m_fileActions[index]->setVisible(true);
}

void
ChronoShareGui::changeSettings()
{
  QString oldUsername = m_username;
  QString oldSharedFolderName = m_sharedFolderName;

  if (!editUsername->text().isEmpty())
    m_username = editUsername->text().trimmed();
  else
    editUsername->setText(m_username);

  if (!editSharedFolder->text().isEmpty())
    m_sharedFolderName = editSharedFolder->text().trimmed();
  else
    editSharedFolder->setText(m_sharedFolderName);

  if (m_username.isNull() || m_username == "" || m_sharedFolderName.isNull() ||
      m_sharedFolderName == "") {
    openMessageBox("Error", "Username and shared folder name cannot be empty");
  }
  else {
    saveSettings();
    this->hide();

    if (m_username != oldUsername || oldSharedFolderName != m_sharedFolderName) {
      startBackend(true); // restart dispatcher/fswatcher
    }
  }
}

void
ChronoShareGui::openFileDialog()
{
  while (true) {
    // prompt user for new directory
    QString tempPath =
      QFileDialog::getExistingDirectory(this, tr("Choose ChronoShare folder"), m_dirPath,
                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (tempPath.isNull()) {
      if (m_dirPath.isNull()) {
        openMessageBox("Notice",
                       "ChronoShare will use [" + QDir::homePath() +
                         "/ChronoShare]. \n\nYou can change it later selecting \"Change Folder\" menu.");

        m_dirPath = QDir::homePath() + "/ChronoShare";
        editSharedFolderPath->setText(m_dirPath);
        break;
      }
      else {
        // user just cancelled, no need to do anything else
        return;
      }
    }

    QFileInfo qFileInfo(tempPath);

    if (!qFileInfo.isDir()) {
      openMessageBox("Error", "Please select an existing folder or create a new one.");
      continue;
    }

    if (m_dirPath == tempPath) {
      // user selected the same directory, no need to do anything
      return;
    }

    m_dirPath = tempPath;
    editSharedFolderPath->setText(m_dirPath);
    break;
  }

  _LOG_DEBUG("Selected path: " << m_dirPath.toStdString());
  // save settings
  saveSettings();

  startBackend(true); // restart dispatcher/fswatcher
}

void
ChronoShareGui::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
  // if double clicked, open shared folder
  if (reason == QSystemTrayIcon::DoubleClick) {
    openSharedFolder();
  }
}

void
ChronoShareGui::viewSettings()
{
  //simple for now
  this->show();
  this->raise();
  this->activateWindow();
}

bool
ChronoShareGui::loadSettings()
{
  bool successful = true;

  // Load Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings;

  _LOG_DEBUG("load settings");

  if (settings.contains("username")) {
    m_username = settings.value("username", "admin").toString();
  }
  else {
    successful = false;
  }

  editUsername->setText(m_username);

  if (settings.contains("sharedfoldername")) {
    m_sharedFolderName = settings.value("sharedfoldername", "shared").toString();
  }
  else {
    successful = false;
  }

  editSharedFolder->setText(m_sharedFolderName);

  if (settings.contains("dirPath")) {
    m_dirPath = settings.value("dirPath", QDir::homePath()).toString();
  }
  else {
    successful = false;
  }

  editSharedFolderPath->setText(m_dirPath);

  _LOG_DEBUG("Found configured path: " << (successful ? m_dirPath.toStdString() : std::string("no")));

  return successful;
}

void
ChronoShareGui::saveSettings()
{
  // Save Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings;

  settings.setValue("dirPath", m_dirPath);
  settings.setValue("username", m_username);
  settings.setValue("sharedfoldername", m_sharedFolderName);
}

void
ChronoShareGui::closeEvent(QCloseEvent* event)
{
  _LOG_DEBUG("Close Event");

  if (m_username.isNull() || m_username == "" || m_sharedFolderName.isNull() ||
      m_sharedFolderName == "") {
    openMessageBox("ChronoShare is not active",
                   "Username and/or shared folder name are not set.\n\n To activate ChronoShare, "
                   "open Settings menu and configure your username and shared folder name");
  }

  this->hide();
  event->ignore(); // don't let the event propagate to the base class
}

#ifdef AUTOUPDATE
void
ChronoShareGui::checkForUpdates()
{
  m_sparkle.checkForUpdates();
}
#endif // AUTOUPDATE

} // namespace chronoshare
} // namespace ndn

#include "chronosharegui.moc"
