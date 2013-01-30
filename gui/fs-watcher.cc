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
 */

#include "fs-watcher.h"
#include "logging.h"

#include <boost/bind.hpp>

#include <QDirIterator>
#include <QRegExp>

using namespace std;
using namespace boost;

INIT_LOGGER ("FsWatcher");

FsWatcher::FsWatcher (QString dirPath,
                      LocalFile_Change_Callback onChange, LocalFile_Change_Callback onDelete,
                      FileState *fileState,
                      QObject* parent)
  : QObject(parent)
  , m_watcher (new QFileSystemWatcher())
  , m_scheduler (new Scheduler ())
  , m_dirPath (dirPath)
  , m_onChange (onChange)
  , m_onDelete (onDelete)
  , m_fileState (fileState)
{
  _LOG_DEBUG ("Monitor dir: " << m_dirPath.toStdString ());
  // add main directory to monitor

  m_watcher->addPath (m_dirPath);

  // register signals (callback functions)
  connect (m_watcher, SIGNAL (directoryChanged (QString)), this, SLOT (DidDirectoryChanged (QString)));
  connect (m_watcher, SIGNAL (fileChanged (QString)),      this, SLOT (DidFileChanged (QString)));

  m_scheduler->start ();

  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind (&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, m_dirPath),
                                  "r-" + m_dirPath.toStdString ()); // only one task will be scheduled per directory

  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, m_dirPath, true),
                                  m_dirPath.toStdString ()); // only one task will be scheduled per directory
}

FsWatcher::~FsWatcher()
{
  m_scheduler->shutdown ();
}

void
FsWatcher::DidDirectoryChanged (QString dirPath)
{
  _LOG_DEBUG ("Triggered DirPath: " << dirPath.toStdString ());

  filesystem::path absPathTriggeredDir (dirPath.toStdString ());
  if (!filesystem::exists (filesystem::path (absPathTriggeredDir)))
    {
      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, dirPath),
                                      "r-" + dirPath.toStdString ()); // only one task will be scheduled per directory
    }
  else
    {
      // m_executor.execute (bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));
      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath, false),
                                      dirPath.toStdString ()); // only one task will be scheduled per directory

      // m_executor.execute (bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));
      Scheduler::scheduleOneTimeTask (m_scheduler, 300,
                                      bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath, true),
                                      "rescan-"+dirPath.toStdString ()); // only one task will be scheduled per directory
    }
}

void
FsWatcher::DidFileChanged (QString filePath)
{
  if (!filePath.startsWith (m_dirPath))
    {
      _LOG_ERROR ("Got notification about a file not from the monitored directory");
      return;
    }
  filesystem::path absPathTriggeredFile (filePath.toStdString ());
  filePath.remove (0, m_dirPath.size ());

  filesystem::path triggeredFile (filePath.toStdString ());
  if (filesystem::exists (filesystem::path (absPathTriggeredFile)))
    {
      _LOG_DEBUG ("Triggered UPDATE of file:  " << triggeredFile.relative_path ().generic_string ());
      // m_onChange (triggeredFile.relative_path ());

      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (m_onChange, triggeredFile.relative_path ()),
                                      triggeredFile.relative_path ().string());
    }
  else
    {
      _LOG_DEBUG ("Triggered DELETE of file: " << triggeredFile.relative_path ().generic_string ());
      // m_onDelete (triggeredFile.relative_path ());

      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (m_onDelete, triggeredFile.relative_path ()),
                                      "r-" + triggeredFile.relative_path ().string());
    }
}

void
FsWatcher::ScanDirectory_NotifyUpdates_Execute (QString dirPath, bool notifyCallbacks)
{
  _LOG_TRACE (" >> ScanDirectory_NotifyUpdates_Execute");

  // exclude working only on last component, not the full path; iterating through all directories, even excluded from monitoring
  QRegExp exclude ("^(\\.|\\.\\.|\\.chronoshare|.*~|.*\\.swp)$");

  QDirIterator dirIterator (dirPath,
                            QDir::Dirs | QDir::Files | /*QDir::Hidden |*/ QDir::NoSymLinks | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories); // directory iterator (recursive)

  // iterate through directory recursively
  while (dirIterator.hasNext ())
    {
      dirIterator.next ();

      // Get FileInfo
      QFileInfo fileInfo = dirIterator.fileInfo ();

      QString name = fileInfo.fileName ();
      _LOG_DEBUG ("+++ Scanning: " <<  name.toStdString ());

      if (!exclude.exactMatch (name))
        {
          _LOG_DEBUG ("Not excluded file/dir: " << fileInfo.absoluteFilePath ().toStdString ());
          QString absFilePath = fileInfo.absoluteFilePath ();

          // _LOG_DEBUG ("Attempt to add path to watcher: " << absFilePath.toStdString ());
          m_watcher->addPath (absFilePath);

          if (notifyCallbacks && fileInfo.isFile ())
            {
              DidFileChanged (absFilePath);
            }
        }
      else
        {
          // _LOG_DEBUG ("Excluded file/dir: " << fileInfo.filePath ().toStdString ());
        }
    }
}


void
FsWatcher::ScanDirectory_NotifyRemovals_Execute (QString dirPath)
{
  _LOG_DEBUG ("Triggered DirPath: " << dirPath.toStdString ());

  filesystem::path absPathTriggeredDir (dirPath.toStdString ());
  dirPath.remove (0, m_dirPath.size ());

  filesystem::path triggeredDir (dirPath.toStdString ());

  FileItemsPtr files = m_fileState->LookupFilesInFolderRecursively (triggeredDir.relative_path ().generic_string ());
  for (std::list<FileItem>::iterator file = files->begin (); file != files->end (); file ++)
    {
      filesystem::path testFile = filesystem::path (m_dirPath.toStdString ()) / file->filename ();
      _LOG_DEBUG ("Check file for deletion [" << testFile.generic_string () << "]");

      if (!filesystem::exists (testFile))
        {
          _LOG_DEBUG ("Notifying about removed file [" << file->filename () << "]");
          m_onDelete (file->filename ());
        }
    }
}

// std::vector<sEventInfo> FsWatcher::reconcileDirectory(QHash<QString, qint64> currentState, QString dirPath)
// {
//   // list of files changed
//   std::vector<sEventInfo> dirChanges;

//   // compare result (database/stored snapshot) to fileList (current snapshot)
//   QMutableHashIterator<QString, qint64> i(m_storedState);

//   while(i.hasNext())
//     {
//       i.next();

//       QString absFilePath = i.key();
//       qint64 storedCreated = i.value();

//       // if this file is in a level higher than
//       // this directory, ignore
//       if(!absFilePath.startsWith(dirPath))
//         {
//           continue;
//         }

//       // check file existence
//       if(currentState.contains(absFilePath))
//         {
//           qint64 currentCreated = currentState.value(absFilePath);

//           if(storedCreated != currentCreated)
//             {
//               // update stored state
//               i.setValue(currentCreated);

//               // this file has been modified
//               sEventInfo eventInfo;
//               eventInfo.event = MODIFIED;
//               eventInfo.absFilePath = absFilePath.toStdString();
//               dirChanges.push_back(eventInfo);
//             }

//           // delete this file from fileList we have processed it
//           currentState.remove(absFilePath);
//         }
//       else
//         {
//           // delete from stored state
//           i.remove();

//           // this file has been deleted
//           sEventInfo eventInfo;
//           eventInfo.event = DELETED;
//           eventInfo.absFilePath = absFilePath.toStdString();
//           dirChanges.push_back(eventInfo);
//         }
//     }

//   // any files left in fileList have been added
//   for(QHash<QString, qint64>::iterator i = currentState.begin(); i != currentState.end(); ++i)
//     {
//       QString absFilePath = i.key();
//       qint64 currentCreated = i.value();

//       m_storedState.insert(absFilePath, currentCreated);

//       // this file has been added
//       sEventInfo eventInfo;
//       eventInfo.event = ADDED;
//       eventInfo.absFilePath = absFilePath.toStdString();
//       dirChanges.push_back(eventInfo);
//     }

//   return dirChanges;
// }

// QByteArray FsWatcher::calcChecksum(QString absFilePath)
// {
//   // initialize checksum
//   QCryptographicHash crypto(QCryptographicHash::Md5);

//   // open file
//   QFile file(absFilePath);
//   file.open(QFile::ReadOnly);

//   // calculate checksum
//   while(!file.atEnd())
//     {
//       crypto.addData(file.read(8192));
//     }

//   return crypto.result();
// }

// void FsWatcher::printChanges(std::vector<sEventInfo> dirChanges)
// {
//   if(!dirChanges.empty())
//     {
//       for(size_t i = 0; i < dirChanges.size(); i++)
//         {
//           QString tempString;

//           eEvent event = dirChanges[i].event;
//           QString absFilePath = QString::fromStdString(dirChanges[i].absFilePath);

//           switch(event)
//             {
//             case ADDED:
//               tempString.append("ADDED: ");
//               break;
//             case MODIFIED:
//               tempString.append("MODIFIED: ");
//               break;
//             case DELETED:
//               tempString.append("DELETED: ");
//               break;
//             }

//           tempString.append(absFilePath);

//           _LOG_DEBUG ("\t" << tempString.toStdString ());
//         }
//     }
//   else
//     {
//           _LOG_DEBUG ("\t[EMPTY]");
//     }
// }

#if WAF
#include "fs-watcher.moc"
#include "fs-watcher.cc.moc"
#endif
