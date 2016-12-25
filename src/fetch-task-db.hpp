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
#ifndef FETCH_TASK_DB_H
#define FETCH_TASK_DB_H

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <ccnx-common.h>
#include <ccnx-name.h>
#include <sqlite3.h>

class FetchTaskDb
{
public:
  FetchTaskDb(const boost::filesystem::path& folder, const std::string& tag);
  ~FetchTaskDb();

  // task with same deviceName and baseName combination will be added only once
  // if task already exists, this call does nothing
  void
  addTask(const Ccnx::Name& deviceName, const Ccnx::Name& baseName, uint64_t minSeqNo,
          uint64_t maxSeqNo, int priority);

  void
  deleteTask(const Ccnx::Name& deviceName, const Ccnx::Name& baseName);

  typedef boost::function<void(const Ccnx::Name&, const Ccnx::Name&, uint64_t, uint64_t, int)>
    FetchTaskCallback;

  void
  foreachTask(const FetchTaskCallback& callback);

private:
  sqlite3* m_db;
};

typedef boost::shared_ptr<FetchTaskDb> FetchTaskDbPtr;

#endif // FETCH_TASK_DB_H
