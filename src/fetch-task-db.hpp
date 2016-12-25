/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
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

#ifndef FETCH_TASK_DB_H
#define FETCH_TASK_DB_H

#include "db-helper.hpp"
#include "core/chronoshare-common.hpp"

#include <ndn-cxx/name.hpp>

#include <sqlite3.h>

#include <boost/filesystem.hpp>

namespace ndn {
namespace chronoshare {

class FetchTaskDb
{
public:
  class Error : public DbHelper::Error
  {
  public:
    explicit Error(const std::string& what)
      : DbHelper::Error(what)
    {
    }
  };

  typedef function<void(const Name&, const Name&, uint64_t, uint64_t, int)> FetchTaskCallback;

public:
  FetchTaskDb(const boost::filesystem::path& folder, const std::string& tag);
  ~FetchTaskDb();

  // task with same deviceName and baseName combination will be added only once
  // if task already exists, this call does nothing
  void
  addTask(const Name& deviceName, const Name& baseName, uint64_t minSeqNo, uint64_t maxSeqNo,
          int priority);

  void
  deleteTask(const Name& deviceName, const Name& baseName);

  void
  foreachTask(const FetchTaskCallback& callback);

private:
  sqlite3* m_db;
};

typedef shared_ptr<FetchTaskDb> FetchTaskDbPtr;

} // namespace chronoshare
} // namespace ndn

#endif // FETCH_TASK_DB_H
