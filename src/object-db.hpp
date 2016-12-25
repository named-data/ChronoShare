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

#ifndef CHRONOSHARE_SRC_OBJECT_DB_HPP
#define CHRONOSHARE_SRC_OBJECT_DB_HPP

#include "db-helper.hpp"
#include "core/chronoshare-common.hpp"

#include <sqlite3.h>

#include <ctime>
#include <vector>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/name.hpp>

#include <boost/filesystem.hpp>

namespace ndn {
namespace chronoshare {

class ObjectDb
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

public:
  // database will be create in <folder>/<first-pair-of-hash-bytes>/<rest-of-hash>
  ObjectDb(const boost::filesystem::path& folder, const std::string& hash);

  ~ObjectDb();

  void
  saveContentObject(const Name& deviceName, sqlite3_int64 segment, const Data& data);

  shared_ptr<Data>
  fetchSegment(const Name& deviceName, sqlite3_int64 segment);

  const time::steady_clock::TimePoint&
  getLastUsed() const;

  static bool
  doesExist(const boost::filesystem::path& folder, const Name& deviceName, const std::string& hash);

private:
  void
  willStartSave();

  void
  didStopSave();

private:
  sqlite3* m_db;
  time::steady_clock::TimePoint m_lastUsed;
};

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_OBJECT_DB_HPP
