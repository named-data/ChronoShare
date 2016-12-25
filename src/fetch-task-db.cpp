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

#include "fetch-task-db.hpp"
#include "db-helper.hpp"

namespace ndn {
namespace chronoshare {

namespace fs = boost::filesystem;

const std::string INIT_DATABASE = "\
CREATE TABLE IF NOT EXISTS                                      \n\
  Task(                                                         \n\
    deviceName  BLOB NOT NULL,                                  \n\
    baseName    BLOB NOT NULL,                                  \n\
    minSeqNo    INTEGER,                                        \n\
    maxSeqNo    INTEGER,                                        \n\
    priority    INTEGER,                                        \n\
    PRIMARY KEY (deviceName, baseName)                          \n\
  );                                                            \n\
CREATE INDEX identifier ON Task (deviceName, baseName);         \n\
";

FetchTaskDb::FetchTaskDb(const boost::filesystem::path& folder, const std::string& tag)
{
  fs::path actualFolder = folder / ".chronoshare" / "fetch_tasks";
  fs::create_directories(actualFolder);

  int res = sqlite3_open((actualFolder / tag).c_str(), &m_db);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot open database: " + (actualFolder / tag).string()));
  }

  char* errmsg = 0;
  res = sqlite3_exec(m_db, INIT_DATABASE.c_str(), NULL, NULL, &errmsg);
  if (res != SQLITE_OK && errmsg != 0) {
    sqlite3_free(errmsg);
  }
  else {
  }
}

FetchTaskDb::~FetchTaskDb()
{
  int res = sqlite3_close(m_db);
  if (res != SQLITE_OK) {
    // _LOG_ERROR
  }
}

void
FetchTaskDb::addTask(const Name& deviceName, const Name& baseName, uint64_t minSeqNo,
                     uint64_t maxSeqNo, int priority)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db,
                     "INSERT OR IGNORE INTO Task(deviceName, baseName, minSeqNo, maxSeqNo, priority) VALUES(?, ?, ?, ?, ?)",
                     -1, &stmt, 0);

  sqlite3_bind_blob(stmt, 1, deviceName.wireEncode().wire(), deviceName.wireEncode().size(),
                    SQLITE_STATIC);
  sqlite3_bind_blob(stmt, 2, baseName.wireEncode().wire(), baseName.wireEncode().size(),
                    SQLITE_STATIC);
  sqlite3_bind_int64(stmt, 3, minSeqNo);
  sqlite3_bind_int64(stmt, 4, maxSeqNo);
  sqlite3_bind_int(stmt, 5, priority);
  int res = sqlite3_step(stmt);

  if (res == SQLITE_OK) {
  }
  sqlite3_finalize(stmt);
}

void
FetchTaskDb::deleteTask(const Name& deviceName, const Name& baseName)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "DELETE FROM Task WHERE deviceName = ? AND baseName = ?;", -1, &stmt, 0);

  sqlite3_bind_blob(stmt, 1, deviceName.wireEncode().wire(), deviceName.wireEncode().size(),
                    SQLITE_STATIC);
  sqlite3_bind_blob(stmt, 2, baseName.wireEncode().wire(), baseName.wireEncode().size(),
                    SQLITE_STATIC);

  int res = sqlite3_step(stmt);
  if (res == SQLITE_OK) {
  }
  sqlite3_finalize(stmt);
}

void
FetchTaskDb::foreachTask(const FetchTaskCallback& callback)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "SELECT * FROM Task;", -1, &stmt, 0);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Name deviceName(Block(sqlite3_column_blob(stmt, 0), sqlite3_column_bytes(stmt, 0)));
    Name baseName(Block(sqlite3_column_blob(stmt, 1), sqlite3_column_bytes(stmt, 1)));

    std::cout << "deviceName: " << deviceName << " baseName: " << baseName << std::endl;
    uint64_t minSeqNo = sqlite3_column_int64(stmt, 2);
    uint64_t maxSeqNo = sqlite3_column_int64(stmt, 3);
    int priority = sqlite3_column_int(stmt, 4);
    callback(deviceName, baseName, minSeqNo, maxSeqNo, priority);
  }

  sqlite3_finalize(stmt);
}

} // namespace chronoshare
} // namespace ndn
