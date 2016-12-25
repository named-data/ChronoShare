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

#include "object-db.hpp"
#include "db-helper.hpp"
#include "core/logging.hpp"

#include <iostream>
#include <sys/stat.h>

#include <ndn-cxx/util/sqlite3-statement.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(ObjectDb);

namespace fs = boost::filesystem;
using ndn::util::Sqlite3Statement;

const std::string INIT_DATABASE = R"SQL(
CREATE TABLE
   File(
        device_name     BLOB NOT NULL,
        segment         INTEGER,
        content_object  BLOB,

        PRIMARY KEY (device_name, segment)
    );
CREATE INDEX device ON File(device_name);
)SQL";

ObjectDb::ObjectDb(const fs::path& folder, const std::string& hash)
  : m_lastUsed(time::steady_clock::now())
{
  fs::path actualFolder = folder / "objects" / hash.substr(0, 2);
  fs::create_directories(actualFolder);

  _LOG_DEBUG("Open " << (actualFolder / hash.substr(2, hash.size() - 2)));

  int res = sqlite3_open((actualFolder / hash.substr(2, hash.size() - 2)).c_str(), &m_db);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot open/create database: [" +
                                (actualFolder / hash.substr(2, hash.size() - 2)).string() + "]"));
  }

  // Alex: determine if tables initialized. if not, initialize... not sure what is the best way to go...
  // for now, just attempt to create everything

  char* errmsg = 0;
  res = sqlite3_exec(m_db, INIT_DATABASE.c_str(), nullptr, nullptr, &errmsg);
  if (res != SQLITE_OK && errmsg != 0) {
    sqlite3_free(errmsg);
  }

  willStartSave();
}

ObjectDb::~ObjectDb()
{
  didStopSave();

  int res = sqlite3_close(m_db);
  if (res != SQLITE_OK) {
    // complain
  }
}

bool
ObjectDb::doesExist(const boost::filesystem::path& folder, const Name& deviceName,
                    const std::string& hash)
{
  BOOST_ASSERT(hash.size() > 2);

  fs::path actualFolder = folder / "objects" / hash.substr(0, 2);
  bool retval = false;

  sqlite3* db;
  int res = sqlite3_open((actualFolder / hash.substr(2, hash.size() - 2)).c_str(), &db);
  if (res == SQLITE_OK) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
                       "SELECT count(*), count(nullif(content_object,0)) FROM File WHERE device_name=?",
                       -1, &stmt, 0);

    sqlite3_bind_blob(stmt, 1, deviceName.wireEncode().wire(), deviceName.wireEncode().size(),
                      SQLITE_TRANSIENT);

    int res = sqlite3_step(stmt);
    if (res == SQLITE_ROW) {
      int countAll = sqlite3_column_int(stmt, 0);
      int countNonNull = sqlite3_column_int(stmt, 1);

      _LOG_TRACE("Total segments: " << countAll << ", non-empty segments: " << countNonNull);

      if (countAll > 0 && countAll == countNonNull) {
        retval = true;
      }
    }

    sqlite3_finalize(stmt);
  }

  sqlite3_close(db);
  return retval;
}

void
ObjectDb::saveContentObject(const Name& deviceName, sqlite3_int64 segment, const Data& data)
{
  // update last used time
  m_lastUsed = time::steady_clock::now();

  Sqlite3Statement stmt(m_db, "INSERT INTO File "
                                "(device_name, segment, content_object) "
                                "VALUES (?, ?, ?)");

  _LOG_DEBUG("Saving content object for [" << deviceName << ", seqno: " << segment << ", size: "
                                           << data.wireEncode().size()
                                           << "]");
  stmt.bind(1, deviceName.wireEncode(), SQLITE_STATIC);
  stmt.bind(2, segment);
  stmt.bind(3, data.wireEncode(), SQLITE_STATIC);
  stmt.step();
}

shared_ptr<Data>
ObjectDb::fetchSegment(const Name& deviceName, sqlite3_int64 segment)
{
  // update last used time
  m_lastUsed = time::steady_clock::now();

  Sqlite3Statement stmt(m_db, "SELECT content_object FROM File WHERE device_name=? AND segment=?");
  stmt.bind(1, deviceName.wireEncode(), SQLITE_STATIC);
  stmt.bind(2, segment);

  if (stmt.step() == SQLITE_ROW) {
    return make_shared<Data>(stmt.getBlock(0));
  }
  else {
    return nullptr;
  }
}

const time::steady_clock::TimePoint&
ObjectDb::getLastUsed() const
{
  return m_lastUsed;
}

void
ObjectDb::willStartSave()
{
  sqlite3_exec(m_db, "BEGIN TRANSACTION;", 0, 0, 0);
}

void
ObjectDb::didStopSave()
{
  sqlite3_exec(m_db, "END TRANSACTION;", 0, 0, 0);
}

} // namespace chronoshare
} // namespace ndn
