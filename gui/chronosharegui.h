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

#ifndef CHRONOSHAREGUI_H
#define CHRONOSHAREGUI_H

#include <QtGui>
#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QFileDialog>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>

#include "fs-watcher.h"

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
  void openSharedFolder();

  // open file dialog
  void openFileDialog();

  // handle left click of tray icon
  void trayIconClicked(QSystemTrayIcon::ActivationReason reason);

  // view chronoshare settings
  void viewSettings();

  // change chronoshare settings
  void changeSettings();

private:
  // create actions that result from clicking a menu option
  void createActions();

  // create tray icon
  void createTrayIcon();

  // set icon image
  void setIcon();

  // load persistent settings
  bool loadSettings();

  // save persistent settings
  void saveSettings();

  // prompt user dialog box
  void openMessageBox(QString title, QString text);

  // overload
  void openMessageBox(QString title, QString text, QString infotext);

  // capture close event
  void closeEvent(QCloseEvent* event);

private:
  QSystemTrayIcon* m_trayIcon; // tray icon
  QMenu* m_trayIconMenu; // tray icon menu

  QAction* m_openFolder; // open the shared folder action
  QAction* m_viewSettings; // chronoShare settings
  QAction* m_changeFolder; // change the shared folder action
  QAction* m_quitProgram; // quit program action

  QString m_dirPath; // shared directory
  QString m_username; // username
  QString m_sharedFolderName; // shared folder name

  FsWatcher* m_watcher;

  QLabel* labelUsername;
  QPushButton* button;
  QLabel* labelSharedFolder;
  QLabel* labelSharedFolderPath;
  QLineEdit* editUsername;
  QLineEdit* editSharedFolder;
  QLineEdit* editSharedFolderPath;
  QLabel *label;
  QVBoxLayout *mainLayout;

  // QString m_settingsFilePath; // settings file path
  // QString m_settings;
};

#endif // CHRONOSHAREGUI_H
