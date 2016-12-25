/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
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

#ifndef CHRONOSHAREGUI_H
#define CHRONOSHAREGUI_H

#include "adhoc.hpp"

#if __APPLE__ && HAVE_SPARKLE
#define SPARKLE_SUPPORTED 1
#include "sparkle-auto-update.hpp"
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QWidget>
#include <QtGui>

#include "dispatcher.hpp"
#include "fs-watcher.hpp"
#include "server.hpp"
#include <boost/thread/thread.hpp>

class ChronoShareGui : public QDialog
{
  Q_OBJECT

public:
  // constructor
  explicit ChronoShareGui(QWidget* parent = 0);

  // destructor
  ~ChronoShareGui();

private slots:
  // open the shared folder
  void
  openSharedFolder();

  void
  openFile();

  void
  openInWebBrowser();

  void
  updateRecentFilesMenu();

  // open file dialog
  void
  openFileDialog();

  // handle left click of tray icon
  void
  trayIconClicked(QSystemTrayIcon::ActivationReason reason);

  // view chronoshare settings
  void
  viewSettings();

  // change chronoshare settings
  void
  changeSettings();

  // click on adhoc button
  void
  onAdHocChange(bool state); // cannot be protected with #ifdef. otherwise something fishy with QT

  void
  onCheckForUpdates();

private:
  void
  checkFileAction(const std::string&, int, int);
  // create actions that result from clicking a menu option
  void
  createActionsAndMenu();

  // create tray icon
  void
  createTrayIcon();

  // set icon image
  void
  setIcon();

  // load persistent settings
  bool
  loadSettings();

  // save persistent settings
  void
  saveSettings();

  // prompt user dialog box
  void
  openMessageBox(QString title, QString text);

  // overload
  void
  openMessageBox(QString title, QString text, QString infotext);

  // capture close event
  void
  closeEvent(QCloseEvent* event);

  // starts/restarts fs watcher and dispatcher
  void
  startBackend(bool restart = false);

private:
  QSystemTrayIcon* m_trayIcon; // tray icon
  QMenu* m_trayIconMenu;       // tray icon menu

  QAction* m_openFolder;   // open the shared folder action
  QAction* m_viewSettings; // chronoShare settings
  QAction* m_changeFolder; // change the shared folder action
  QAction* m_quitProgram;  // quit program action
  QAction* m_checkForUpdates;
  QAction* m_openWeb;
  QMenu* m_recentFilesMenu;
  QAction* m_fileActions[5];

  QAction* m_wifiAction;

  QString m_dirPath;          // shared directory
  QString m_username;         // username
  QString m_sharedFolderName; // shared folder name

  FsWatcher* m_watcher;
  Dispatcher* m_dispatcher;
  http::server::server* m_httpServer;
  boost::thread m_httpServerThread;

  QLabel* labelUsername;
  QPushButton* button;
  QLabel* labelSharedFolder;
  QLabel* labelSharedFolderPath;
  QLineEdit* editUsername;
  QLineEdit* editSharedFolder;
  QLineEdit* editSharedFolderPath;
  QLabel* label;
  QVBoxLayout* mainLayout;

#ifdef ADHOC_SUPPORTED
  Executor m_executor;
#endif

#ifdef SPARKLE_SUPPORTED
  AutoUpdate* m_autoUpdate;
#endif
  // QString m_settingsFilePath; // settings file path
  // QString m_settings;
};

#endif // CHRONOSHAREGUI_H
