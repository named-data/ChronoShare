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

#ifndef FILE_STATE_H
#define FILE_STATE_H

#include "db-helper.hpp"

#include "ccnx-name.hpp"
#include "file-item.pb.hpp"
#include "hash-helper.hpp"

#include <boost/exception/all.hpp>
#include <boost/tuple/tuple.hpp>

#include <list>

typedef std::list<FileItem> FileItems;
typedef boost::shared_ptr<FileItem> FileItemPtr;
typedef boost::shared_ptr<FileItems> FileItemsPtr;


class FileState : public DbHelper
{
public:
  FileState(const boost::filesystem::path& path);
  ~FileState();

  /**
   * @brief Update or add a file
   */
  void
  UpdateFile(const std::string& filename, sqlite3_int64 version, const Hash& hash,
             const Ccnx::CcnxCharbuf& device_name, sqlite3_int64 seqno, time_t atime, time_t mtime,
             time_t ctime, int mode, int seg_num);

  /**
   * @brief Delete file
   */
  void
  DeleteFile(const std::string& filename);

  /**
   * @brief Set "complete" flag
   *
   * The call will do nothing if FileState does not have a record for the file (e.g., file got subsequently deleted)
   */
  void
  SetFileComplete(const std::string& filename);

  /**
   * @brief Lookup file state using file name
   */
  FileItemPtr
  LookupFile(const std::string& filename);

  /**
   * @brief Lookup file state using content hash (multiple items may be returned)
   */
  FileItemsPtr
  LookupFilesForHash(const Hash& hash);

  /**
   * @brief Lookup all files in the specified folder and call visitor(file) for each file
   */
  void
  LookupFilesInFolder(const boost::function<void(const FileItem&)>& visitor,
                      const std::string& folder, int offset = 0, int limit = -1);

  /**
   * @brief Lookup all files in the specified folder (wrapper around the overloaded version)
   */
  FileItemsPtr
  LookupFilesInFolder(const std::string& folder, int offset = 0, int limit = -1);

  /**
   * @brief Recursively lookup all files in the specified folder and call visitor(file) for each file
   */
  bool
  LookupFilesInFolderRecursively(const boost::function<void(const FileItem&)>& visitor,
                                 const std::string& folder, int offset = 0, int limit = -1);

  /**
   * @brief Recursively lookup all files in the specified folder (wrapper around the overloaded version)
   */
  FileItemsPtr
  LookupFilesInFolderRecursively(const std::string& folder, int offset = 0, int limit = -1);
};

typedef boost::shared_ptr<FileState> FileStatePtr;

namespace Error {
struct FileState : virtual boost::exception, virtual std::exception
{
};
}


#endif // ACTION_LOG_H
