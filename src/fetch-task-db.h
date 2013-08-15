/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */
#ifndef FETCH_TASK_DB_H
#define FETCH_TASK_DB_H

#include <sqlite3.h>
#include <ndnx-common.h>
#include <ndnx-name.h>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

class FetchTaskDb
{
public:
  FetchTaskDb(const boost::filesystem::path &folder, const std::string &tag);
  ~FetchTaskDb();

  // task with same deviceName and baseName combination will be added only once
  // if task already exists, this call does nothing
  void
  addTask(const Ndnx::Name &deviceName, const Ndnx::Name &baseName, uint64_t minSeqNo, uint64_t maxSeqNo, int priority);

  void
  deleteTask(const Ndnx::Name &deviceName, const Ndnx::Name &baseName);

  typedef boost::function<void(const Ndnx::Name &, const Ndnx::Name &, uint64_t, uint64_t, int)> FetchTaskCallback;

  void
  foreachTask(const FetchTaskCallback &callback);

private:
  sqlite3 *m_db;
};

typedef boost::shared_ptr<FetchTaskDb> FetchTaskDbPtr;

#endif // FETCH_TASK_DB_H
