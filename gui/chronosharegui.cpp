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
 */

#include "chronosharegui.h"
#include "logging.h"

INIT_LOGGER ("Gui");

ChronoShareGui::ChronoShareGui(QWidget *parent)
  : QWidget(parent)
  // , m_settingsFilePath(QDir::homePath() + "/.chronoshare")
{
  // load settings
  if(!loadSettings())
    {
      // prompt user to choose folder
      openMessageBox("First Time Setup", "Please select your shared folder location.");
      openFileDialog();
    }

  // create actions that result from clicking a menu option
  createActions();

  // create tray icon
  createTrayIcon();

  // set icon image
  setIcon();

  // show tray icon
  m_trayIcon->show();
}

ChronoShareGui::~ChronoShareGui()
{
  // cleanup
  delete m_trayIcon;
  delete m_trayIconMenu;
  delete m_openFolder;
  delete m_viewSettings;
  delete m_changeFolder;
  delete m_quitProgram;
}

void ChronoShareGui::openMessageBox(QString title, QString text)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);

  messageBox.setIconPixmap(QPixmap(":/images/friends-group-icon.png"));

  messageBox.exec();
}

void ChronoShareGui::openMessageBox(QString title, QString text, QString infotext)
{
  QMessageBox messageBox(this);
  messageBox.setWindowTitle(title);
  messageBox.setText(text);
  messageBox.setInformativeText(infotext);

  messageBox.setIconPixmap(QPixmap(":/images/friends-group-icon.png"));

  messageBox.exec();
}

void ChronoShareGui::createActions()
{
  // create the "open folder" action
  m_openFolder = new QAction(tr("&Open Folder"), this);
  connect(m_openFolder, SIGNAL(triggered()), this, SLOT(openSharedFolder()));

  // create the "view settings" action
  m_viewSettings = new QAction(tr("&View Settings"), this);
  connect(m_viewSettings, SIGNAL(triggered()), this, SLOT(viewSettings()));

  // create the "change folder" action
  m_changeFolder = new QAction(tr("&Change Folder"), this);
  connect(m_changeFolder, SIGNAL(triggered()), this, SLOT(openFileDialog()));

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
  m_trayIconMenu->addAction(m_quitProgram);

  // create new tray icon
  m_trayIcon = new QSystemTrayIcon(this);

  // associate the menu with the tray icon
  m_trayIcon->setContextMenu(m_trayIconMenu);

  // handle left click of icon
  connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));
}

void ChronoShareGui::setIcon()
{
  // set the icon image
  m_trayIcon->setIcon(QIcon(":/images/friends-group-icon.png"));
}

void ChronoShareGui::openSharedFolder()
{
  // Alex: isn't there an OS-independent way in QT for this?

  // tell Finder to open the shared folder
  QStringList scriptArgs;
  scriptArgs << QLatin1String("-e")
             << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
    .arg(m_dirPath);

  // execute the commands to make it happen
  QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);

  // clear command arguments
  scriptArgs.clear();

  // tell Finder to appear
  scriptArgs << QLatin1String("-e")
             << QLatin1String("tell application \"Finder\" to activate");

  // execute the commands to make it happen
  QProcess::execute("/usr/bin/osascript", scriptArgs);
}

void ChronoShareGui::openFileDialog()
{
  // prompt user for new directory
  QString tempPath = QFileDialog::getExistingDirectory(this, tr("Choose a new folder"),
                                                       m_dirPath, QFileDialog::ShowDirsOnly |
                                                       QFileDialog::DontResolveSymlinks);
  QFileInfo qFileInfo(tempPath);

  if(qFileInfo.isDir())
    m_dirPath = tempPath;
  else
    openMessageBox ("Error", "Not a valid folder, Ignoring.");

  _LOG_DEBUG ("Selected path: " << m_dirPath.toStdString ());

  openMessageBox("Current Folder", "Current Shared Folder:\n" + m_dirPath, "You may change the folder by selecting \"change folder\" from the icon in the system tray.");

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
  // simple for now
  openMessageBox("Chronoshare Settings", "CurrentFolder:\n" + m_dirPath);
}

bool ChronoShareGui::loadSettings()
{
  bool successful = false;

  // Load Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings (QSettings::NativeFormat, QSettings::UserScope, "irl.cs.ucla.edu", "ChronoShare");

  // _LOG_DEBUG (lexical_cast<string> (settings.allKeys()));

  if(settings.contains("dirPath"))
    {
      m_dirPath = settings.value("dirPath", QDir::homePath()).toString();
      successful = true;
    }
  else
    {
      m_dirPath = QDir::homePath();
      successful = false;
    }

  _LOG_DEBUG ("Found configured path: " << (successful? m_dirPath.toStdString () : std::string("no")));

  return successful;
}

void ChronoShareGui::saveSettings()
{
  // Save Settings
  // QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
  QSettings settings (QSettings::NativeFormat, QSettings::UserScope, "irl.cs.ucla.edu", "ChronoShare");
  settings.setValue("dirPath", m_dirPath);
}

void ChronoShareGui::closeEvent(QCloseEvent* event)
{
  _LOG_DEBUG ("Close Event")
  event->ignore(); // don't let the event propagate to the base class
}

#if WAF
#include "chronosharegui.moc"
#include "chronosharegui.cpp.moc"
#endif
