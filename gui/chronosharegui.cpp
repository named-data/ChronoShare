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
#include "ui_chronosharegui.h"

ChronoShareGui::ChronoShareGui(QWidget *parent) :
    QWidget(parent),
    m_fileDialogWidget(new QWidget())
{
    // load settings
    loadSettings();

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
    delete m_changeFolder;
    delete m_quitProgram;
    delete m_fileDialogWidget;
}

void ChronoShareGui::createActions()
{
    // create the "open folder" action
    m_openFolder = new QAction(tr("&Open Folder"), this);
    connect(m_openFolder, SIGNAL(triggered()), this, SLOT(openSharedFolder()));

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
    m_dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                    m_dirPath, QFileDialog::ShowDirsOnly |
                                                     QFileDialog::DontResolveSymlinks);

    qDebug() << m_dirPath;

    // save settings
    saveSettings();
}

void ChronoShareGui::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    // if double clicked, open shared folder
    if(reason == QSystemTrayIcon::DoubleClick)
    {
        openSharedFolder();
    }
}

void ChronoShareGui::loadSettings()
{
    // Load Settings
    QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
    m_dirPath = settings.value("dirPath", QDir::homePath()).toString();
}

void ChronoShareGui::saveSettings()
{
    // Save Settings
    QSettings settings(m_settingsFilePath, QSettings::NativeFormat);
    settings.setValue("dirPath", m_dirPath);
}

void ChronoShareGui::closeEvent(QCloseEvent* event)
{
    qDebug() << "Close Event.";
    event->ignore(); // don't let the event propagate to the base class
}
