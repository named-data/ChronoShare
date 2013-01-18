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

ChronoShareGui::ChronoShareGui(QString dirPath, QWidget *parent) :
    QWidget(parent),
    m_dirPath(dirPath)
{
    createActions();
    createTrayIcon();
    setIcon();

    m_trayIcon->show();
}

ChronoShareGui::~ChronoShareGui()
{
    delete m_trayIcon;
    delete m_trayIconMenu;
    delete m_openFolder;
    delete m_quitProgram;
}

void ChronoShareGui::createActions()
{
    m_openFolder = new QAction(tr("&Open"), this);
    connect(m_openFolder, SIGNAL(triggered()), this, SLOT(openSharedFolder()));

    m_quitProgram = new QAction(tr("&Quit"), this);
    connect(m_quitProgram, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void ChronoShareGui::createTrayIcon()
{
    m_trayIconMenu = new QMenu(this);

    m_trayIconMenu->addAction(m_openFolder);
    m_trayIconMenu->addSeparator();
    m_trayIconMenu->addAction(m_quitProgram);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayIconMenu);
}

void ChronoShareGui::setIcon()
{
    m_trayIcon->setIcon(QIcon(":/images/friends-group-icon.png"));
}

void ChronoShareGui::openSharedFolder()
{
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
               << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                                     .arg(m_dirPath);
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e")
               << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute("/usr/bin/osascript", scriptArgs);
}
