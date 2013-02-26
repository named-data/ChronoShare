/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jared Lindblom <lindblom@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Ilya Moiseenko <iliamo@ucla.edu>
 */

#include "chronosharegui.h"

#include "logging.h"
#include "ccnx-wrapper.h"
#include <QValidator>
#include <QDir>
#include <QDesktopServices>

#include <boost/make_shared.hpp>

using namespace boost;
using namespace Ccnx;

static const string HTTP_SERVER_ADDRESS = "localhost";
static const string HTTP_SERVER_PORT = "9001";
static const string DOC_ROOT = ":/html";
static const QString ICON_PICTURE_QSTRING(":/images/friends-group-icon.png");

INIT_LOGGER ("Gui");

ChronoShareGui::ChronoShareGui(QWidget *parent)
  : QDialog(parent)
  , m_watcher(0)
  , m_dispatcher(0)
  , m_httpServer(0)
#ifdef ADHOC_SUPPORTED
  , m_executor (1)
#endif
{
  setWindowTitle("Settings");

  labelUsername = new QLabel("Username (hint: /<username>)");
  labelSharedFolder = new QLabel("Shared Folder Name");
  labelSharedFolderPath = new QLabel("Shared Folder Path");

  QRegExp regex("(/[^/]+)+$");
  QValidator *prefixValidator = new QRegExpValidator(regex, this);

  editUsername = new QLineEdit();
  editUsername->setValidator(prefixValidator);

  QRegExp noPureWhiteSpace("^\\S+.*$");
  QValidator *wsValidator = new QRegExpValidator(noPureWhiteSpace, this);
  editSharedFolder = new QLineEdit();
  editSharedFolder->setValidator(wsValidator);

  editSharedFolderPath = new QLineEdit();
  editSharedFolderPath->setReadOnly(true);
  QPalette pal = editSharedFolderPath->palette();
  pal.setColor(QPalette::Active, QPalette::Base, pal.color(QPalette::Disabled, QPalette::Base));
  editSharedFolderPath->setPalette(pal);
  button = new QPushButton("Submit");
  label = new QLabel();

  connect(button, SIGNAL(clicked()), this, SLOT(changeSettings()));

  mainLayout = new QVBoxLayout; //vertically
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
  createActions();

  // create tray icon
  createTrayIcon();

  // set icon image
  setIcon();

  // show tray icon
  m_trayIcon->show();

  // Dispatcher(const boost::filesystem::path &path, const std::string &localUserName,  const Ccnx::Name &localPrefix,
  //            const std::string &sharedFolder, const boost::filesystem::path &rootDir,
  //            Ccnx::CcnxWrapperPtr ccnx, SchedulerPtr scheduler, int poolSize = 2);

  // load settings
  if(!loadSettings())
    {
      // prompt user to choose folder
      openMessageBox("First Time Setup", "Please enter a username, shared folder name and choose the shared folder path on your local filesystem.");
      viewSettings();
      openFileDialog();
      viewSettings();
    }
  else
    {
      startBackend();
    }

#ifdef ADHOC_SUPPORTED
  m_executor.start ();
#endif
}

void
ChronoShareGui::startBackend()
{
  if (m_watcher != 0 && m_dispatcher != 0)
  {
    return;
  }

  m_dispatcher = new Dispatcher (m_username.toStdString (), m_sharedFolderName.toStdString (),
                                 m_dirPath.toStdString (), make_shared<CcnxWrapper> ());

  // Alex: this **must** be here, otherwise m_dirPath will be uninitialized
  m_watcher = new FsWatcher (m_dirPath,
                             bind (&Dispatcher::Did_LocalFile_AddOrModify, m_dispatcher, _1),
                             bind (&Dispatcher::Did_LocalFile_Delete,      m_dispatcher, _1));
  QFileInfo indexHtmlInfo(":/html/index.html");
  if (indexHtmlInfo.exists())
  {
    m_httpServer = new http::server::server(HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT, DOC_ROOT);
    m_httpServerThread = boost::thread(&http::server::server::run, m_httpServer);
  }
  else
  {
    _LOG_ERROR ("Http server doc root dir does not exist!");
    // shall we bail here?
  }
}

ChronoShareGui::~ChronoShareGui()
{
#ifdef ADHOC_SUPPORTED
  m_executor.shutdown ();
#endif

  delete m_watcher; // stop filewatching ASAP
  delete m_dispatcher; // stop dispatcher ASAP, but after watcher (to prevent triggering callbacks on deleted object)
  delete m_httpServer;

  // cleanup
  delete m_trayIcon;
  delete m_trayIconMenu;
#ifdef ADHOC_SUPPORTED
  delete m_wifiAction;
#endif
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

void ChronoShareGui::openMessageBox(QString title, QString text)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);

  messageBox.setIconPixmap(QPixmap(ICON_PICTURE_QSTRING));

  messageBox.exec();
}

void ChronoShareGui::openMessageBox(QString title, QString text, QString infotext)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);
  messageBox.setInformativeText(infotext);

  messageBox.setIconPixmap(QPixmap(ICON_PICTURE_QSTRING));

  messageBox.exec();
}

void ChronoShareGui::createActions()
{
  _LOG_DEBUG ("Create actions");

  // create the "open folder" action
  m_openFolder = new QAction(tr("&Open Folder"), this);
  connect(m_openFolder, SIGNAL(triggered()), this, SLOT(openSharedFolder()));

  // create the "view settings" action
  m_viewSettings = new QAction(tr("&View Settings"), this);
  connect(m_viewSettings, SIGNAL(triggered()), this, SLOT(viewSettings()));

  // create the "change folder" action
  m_changeFolder = new QAction(tr("&Change Folder"), this);
  connect(m_changeFolder, SIGNAL(triggered()), this, SLOT(openFileDialog()));

#ifdef ADHOC_SUPPORTED
  // create "AdHoc Wifi" action
  m_wifiAction = new QAction (tr("Enable AdHoc &WiFI"), m_trayIconMenu);
  m_wifiAction->setChecked (false);
  m_wifiAction->setCheckable (true);
  connect (m_wifiAction, SIGNAL (toggled(bool)), this, SLOT(onAdHocChange(bool)));
#endif

  // create the "quit program" action
  m_quitProgram = new QAction(tr("&Quit"), this);
  connect(m_quitProgram, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void ChronoShareGui::createTrayIcon()
{
  // create a new icon menu
  m_trayIconMenu = new QMenu(this);

  // add actions to the menu
  m_trayIconMenu->addAction(m_openFolder);
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_viewSettings);
  m_trayIconMenu->addAction(m_changeFolder);
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_wifiAction);
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_quitProgram);

  // create new tray icon
  m_trayIcon = new QSystemTrayIcon(this);

#ifdef ADHOC_SUPPORTED
  // associate the menu with the tray icon
  m_trayIcon->setContextMenu(m_trayIconMenu);
#endif

  // handle left click of icon
  connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));
}

void
ChronoShareGui::onAdHocChange (bool state)
{
#ifdef ADHOC_SUPPORTED
  if (m_wifiAction->isChecked ())
    {
      QMessageBox msgBox;
      msgBox.setText ("WARNING: your WiFi will be disconnected");
      msgBox.setIcon (QMessageBox::Warning);
      msgBox.setInformativeText("AdHoc WiFi will disconnect your current WiFi.\n  Are you sure that you are OK with that?");
      msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
      msgBox.setDefaultButton(QMessageBox::Cancel);
      int ret = msgBox.exec();

      if (ret == QMessageBox::Cancel)
        {
          m_wifiAction->setChecked (false);
        }
      else
        {

          m_wifiAction->setText (tr("Disable AdHoc WiFI"));

          // create adhoc
          m_executor.execute (Adhoc::CreateAdhoc);
        }
    }
  else
    {
      m_wifiAction->setText (tr("Enable AdHoc WiFI"));

      // disable adhoc
      m_executor.execute (Adhoc::DestroyAdhoc);

      // a trick in DestroyAdhoc ensures that WiFi will be reconnected to a default WiFi
    }
#endif
}

void ChronoShareGui::setIcon()
{
  // set the icon image
  m_trayIcon->setIcon(QIcon(ICON_PICTURE_QSTRING));
}

void ChronoShareGui::openSharedFolder()
{
  QString path = QDir::toNativeSeparators(m_dirPath);
  QDesktopServices::openUrl(QUrl("file:///" + path));
}

void ChronoShareGui::changeSettings()
{
  if(!editUsername->text().isEmpty())
    m_username = editUsername->text().trimmed();
  else
    editUsername->setText(m_username);

  if(!editSharedFolder->text().isEmpty())
    m_sharedFolderName = editSharedFolder->text().trimmed();
  else
    editSharedFolder->setText(m_sharedFolderName);

  saveSettings();
  this->hide();

  startBackend();
}

void ChronoShareGui::openFileDialog()
{
  // prompt user for new directory
  QString tempPath = QFileDialog::getExistingDirectory(this, tr("Choose a new folder"),
                                                       m_dirPath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  QFileInfo qFileInfo (tempPath);

  if(qFileInfo.isDir())
    {
      m_dirPath = tempPath;
      editSharedFolderPath->setText(m_dirPath);
    }
  else
    {
      openMessageBox ("Error", "Not a valid folder, Ignoring.");
    }

  _LOG_DEBUG ("Selected path: " << m_dirPath.toStdString ());

  // save settings
  saveSettings();
}

void ChronoShareGui::trayIconClicked (QSystemTrayIcon::ActivationReason reason)
{
  // if double clicked, open shared folder
  if(reason == QSystemTrayIcon::DoubleClick)
    {
      openSharedFolder();
    }
}

void ChronoShareGui::viewSettings()
{
  //simple for now
  this->show();
  this->raise();
  this->activateWindow();
}

bool ChronoShareGui::loadSettings()
{
  bool successful = true;

  // Load Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings (QSettings::NativeFormat, QSettings::UserScope, "irl.cs.ucla.edu", "ChronoShare");

  // _LOG_DEBUG (lexical_cast<string> (settings.allKeys()));

  if(settings.contains("username"))
    {
      m_username = settings.value("username", "admin").toString();
    }
  else
    {
      successful = false;
    }

  editUsername->setText(m_username);

  if(settings.contains("sharedfoldername"))
    {
      m_sharedFolderName = settings.value("sharedfoldername", "shared").toString();
    }
  else
    {
      successful = false;
    }

  editSharedFolder->setText(m_sharedFolderName);

  if(settings.contains("dirPath"))
    {
      m_dirPath = settings.value("dirPath", QDir::homePath()).toString();
    }
  else
    {
      successful = false;
    }

  editSharedFolderPath->setText(m_dirPath);

  _LOG_DEBUG ("Found configured path: " << (successful ? m_dirPath.toStdString () : std::string("no")));

  return successful;
}

void ChronoShareGui::saveSettings()
{
  // Save Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings (QSettings::NativeFormat, QSettings::UserScope, "irl.cs.ucla.edu", "ChronoShare");

  settings.setValue("dirPath", m_dirPath);
  settings.setValue("username", m_username);
  settings.setValue("sharedfoldername", m_sharedFolderName);
}

void ChronoShareGui::closeEvent(QCloseEvent* event)
{
  _LOG_DEBUG ("Close Event")
    this->hide();
  event->ignore(); // don't let the event propagate to the base class
}

#if WAF
#include "chronosharegui.moc"
#include "chronosharegui.cpp.moc"
#endif
