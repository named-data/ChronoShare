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
#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QFileSystemWatcher>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QDebug>
#include <QHash>
#include <structs.h>

#define DEBUG 1

class FileSystemWatcher : public QObject
{
    Q_OBJECT

public:
    // constructor
    FileSystemWatcher(QString dirPath, QObject* parent = 0);

    // destructor
    ~FileSystemWatcher();

signals:
    // directory event signal
    void dirEventSignal(std::vector<sEventInfo> dirChanges);

private slots:
    // handle callback from watcher
    void watcherCallbackSlot(QString dirPath);

    // handle callback from timer
    void timerCallbackSlot();

    // bootstrap
    void bootstrap();

private:
    // handle callback from either the watcher or timer
    void handleCallback(QString dirPath);

    // scan directory and populate file list
    QHash<QString, qint64> scanDirectory(QString dirPath);

    // reconcile directory, find changes
    std::vector<sEventInfo> reconcileDirectory(QHash<QString, qint64> fileList, QString dirPath);

    // calculate checksum
    QByteArray calcChecksum(QString absFilePath);

    // print Changes (DEBUG)
    void printChanges(std::vector<sEventInfo> dirChanges);

private:
    QFileSystemWatcher* m_watcher; // filesystem watcher
    QTimer* m_timer; // timer

    QString m_dirPath; // monitored path
    QHash<QString, qint64> m_storedState; // stored state of directory
};

#endif // FILESYSTEMWATCHER_H
