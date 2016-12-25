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
#ifndef FS_WATCHER_H
#define FS_WATCHER_H

#include <boost/filesystem.hpp>
#include <QFileSystemWatcher>
#include <sqlite3.h>
#include <vector>

#include "scheduler.hpp"

class FsWatcher : public QObject
{
  Q_OBJECT

public:
  typedef boost::function<void(const boost::filesystem::path&)> LocalFile_Change_Callback;

  // constructor
  FsWatcher(QString dirPath, LocalFile_Change_Callback onChange, LocalFile_Change_Callback onDelete,
            QObject* parent = 0);

  // destructor
  ~FsWatcher();

private slots:
  // handle callback from watcher
  void
  DidDirectoryChanged(QString dirPath);

  /**
   * @brief This even will be triggered either by actual file change or via directory change event
   * (i.e., can happen twice in a row, as well as trigger false alarm)
   */
  void
  DidFileChanged(QString filePath);

private:
  // handle callback from the watcher
  // scan directory and notify callback about any file changes
  void
  ScanDirectory_NotifyUpdates_Execute(QString dirPath);

  void
  ScanDirectory_NotifyRemovals_Execute(QString dirPath);

  void
  initFileStateDb();

  bool
  fileExists(const boost::filesystem::path& filename);

  void
  addFile(const boost::filesystem::path& filename);

  void
  deleteFile(const boost::filesystem::path& filename);

  void
  getFilesInDir(const boost::filesystem::path& dir, std::vector<std::string>& files);

private:
  QFileSystemWatcher* m_watcher; // filesystem watcher
  SchedulerPtr m_scheduler;

  QString m_dirPath; // monitored path

  LocalFile_Change_Callback m_onChange;
  LocalFile_Change_Callback m_onDelete;

  sqlite3* m_db;
};

#endif // FILESYSTEMWATCHER_H
