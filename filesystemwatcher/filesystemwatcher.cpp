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

#include "filesystemwatcher.h"

FileSystemWatcher::FileSystemWatcher(QString dirPath, QObject* parent) :
    QObject(parent),
    m_watcher(new QFileSystemWatcher()),
    m_timer(new QTimer()),
    m_dirPath(dirPath)
{
    // add main directory to monitor
    m_watcher->addPath(m_dirPath);

    // register signals (callback functions)
    connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(watcherCallbackSlot(QString)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerCallbackSlot()));

    // bootstrap
    QTimer::singleShot(1000, this, SLOT(bootstrap()));

    // start timer
    m_timer->start(300000);
}

FileSystemWatcher::~FileSystemWatcher()
{
    // clean up
    delete m_watcher;
    delete m_timer;
}

void FileSystemWatcher::bootstrap()
{
    // bootstrap specific steps
#if DEBUG
    qDebug() << endl << "[BOOTSTRAP]";
#endif
    timerCallbackSlot();
#if DEBUG
    qDebug() << endl << "[\\BOOTSTRAP]";
#endif
}

void FileSystemWatcher::watcherCallbackSlot(QString dirPath)
{
    // watcher specific steps
#if DEBUG
    qDebug() << endl << "[WATCHER] Triggered Path: " << dirPath;
#endif
    handleCallback(dirPath);
}

void FileSystemWatcher::timerCallbackSlot()
{
    // timer specific steps
#if DEBUG
    qDebug() << endl << "[TIMER] Triggered Path: " << m_dirPath;
#endif
    handleCallback(m_dirPath);
}

void FileSystemWatcher::handleCallback(QString dirPath)
{
    // scan directory and populate file list
    QHash<QString, qint64> currentState = scanDirectory(dirPath);

    // reconcile directory and report changes
    std::vector<sEventInfo> dirChanges = reconcileDirectory(currentState, dirPath);
#if DEBUG
    // DEBUG: Print Changes
    printChanges(dirChanges);
#endif
    // emit the signal if not empty
    if(!dirChanges.empty())
        emit dirEventSignal(dirChanges);
}

QHash<QString, qint64> FileSystemWatcher::scanDirectory(QString dirPath)
{   
    // list of files in directory
    QHash<QString, qint64> currentState;

    // directory iterator (recursive)
    QDirIterator dirIterator(dirPath, QDirIterator::Subdirectories |
                                        QDirIterator::FollowSymlinks);

    // iterate through directory recursively
    while(dirIterator.hasNext())
    {
        // Get Next File/Dir
        dirIterator.next();

        // Get FileInfo
        QFileInfo fileInfo = dirIterator.fileInfo();

        // if not this directory or previous directory
        if(fileInfo.absoluteFilePath() != ".." && fileInfo.absoluteFilePath() != ".")
        {
            QString absFilePath = fileInfo.absoluteFilePath();

            // if this is a directory
            if(fileInfo.isDir())
            {
                QStringList dirList = m_watcher->directories();

                // if the directory is not already being watched
                if (absFilePath.startsWith(m_dirPath) && !dirList.contains(absFilePath))
                {
                    // add this directory to the watch list
                    m_watcher->addPath(absFilePath);
                }
            }
            else
            {
                // add this file to the file list
                currentState.insert(absFilePath, fileInfo.created().toMSecsSinceEpoch());
            }
        }
    }

    return currentState;
}

std::vector<sEventInfo> FileSystemWatcher::reconcileDirectory(QHash<QString, qint64> currentState, QString dirPath)
{
    // list of files changed
    std::vector<sEventInfo> dirChanges;

    // compare result (database/stored snapshot) to fileList (current snapshot)
    QMutableHashIterator<QString, qint64> i(m_storedState);

    while(i.hasNext())
    {
        i.next();

        QString absFilePath = i.key();
        qint64 storedCreated = i.value();

        // if this file is in a level higher than
        // this directory, ignore
        if(!absFilePath.startsWith(dirPath))
        {
            continue;
        }

        // check file existence
        if(currentState.contains(absFilePath))
        {
            qint64 currentCreated = currentState.value(absFilePath);

            if(storedCreated != currentCreated)
            {
                // update stored state
                i.setValue(currentCreated);

                // this file has been modified
                sEventInfo eventInfo;
                eventInfo.event = MODIFIED;
                eventInfo.absFilePath = absFilePath.toStdString();
                dirChanges.push_back(eventInfo);
            }

            // delete this file from fileList we have processed it
            currentState.remove(absFilePath);
        }
        else
        {
            // delete from stored state
            i.remove();

            // this file has been deleted
            sEventInfo eventInfo;
            eventInfo.event = DELETED;
            eventInfo.absFilePath = absFilePath.toStdString();
            dirChanges.push_back(eventInfo);
        }
    }

    // any files left in fileList have been added
    for(QHash<QString, qint64>::iterator i = currentState.begin(); i != currentState.end(); ++i)
    {
        QString absFilePath = i.key();
        qint64 currentCreated = i.value();

        m_storedState.insert(absFilePath, currentCreated);

        // this file has been added
        sEventInfo eventInfo;
        eventInfo.event = ADDED;
        eventInfo.absFilePath = absFilePath.toStdString();
        dirChanges.push_back(eventInfo);
    }

    return dirChanges;
}

QByteArray FileSystemWatcher::calcChecksum(QString absFilePath)
{
    // initialize checksum
    QCryptographicHash crypto(QCryptographicHash::Md5);

    // open file
    QFile file(absFilePath);
    file.open(QFile::ReadOnly);

    // calculate checksum
    while(!file.atEnd())
    {
        crypto.addData(file.read(8192));
    }

    return crypto.result();
}

void FileSystemWatcher::printChanges(std::vector<sEventInfo> dirChanges)
{
    if(!dirChanges.empty())
    {
        for(size_t i = 0; i < dirChanges.size(); i++)
        {
            QString tempString;

            eEvent event = dirChanges[i].event;
            QString absFilePath = QString::fromStdString(dirChanges[i].absFilePath);

            switch(event)
            {
            case ADDED:
                tempString.append("ADDED: ");
                break;
            case MODIFIED:
                tempString.append("MODIFIED: ");
                break;
            case DELETED:
                tempString.append("DELETED: ");
                break;
            }

            tempString.append(absFilePath);

            qDebug() << "\t" << tempString;
        }
    }
    else
    {
        qDebug() << "\t[EMPTY]";
    }
}

#if WAF
#include "filesystemwatcher.moc"
#include "filesystemwatcher.cpp.moc"
#endif
