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

#include "db-helper.hpp"
#include "core/logging.hpp"

#include <ndn-cxx/util/digest.hpp>

namespace ndn {
namespace chronoshare {

INIT_LOGGER("DbHelper");

namespace fs = boost::filesystem;

using util::Sha256;

const std::string INIT_DATABASE = "\
    PRAGMA foreign_keys = ON;      \
";

DbHelper::DbHelper(const fs::path& path, const std::string& dbname)
{
  fs::create_directories(path);

  int res = sqlite3_open((path / dbname).c_str(), &m_db);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot open/create database: [" + (path / dbname).string() + "]"));
  }

  res = sqlite3_create_function(m_db, "hash", 2, SQLITE_ANY, 0, 0, DbHelper::hash_xStep,
                                DbHelper::hash_xFinal);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot create function ``hash''"));
  }

  res = sqlite3_create_function(m_db, "is_prefix", 2, SQLITE_ANY, 0, DbHelper::is_prefix_xFun, 0, 0);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot create function ``is_prefix''"));
  }

  res = sqlite3_create_function(m_db, "directory_name", -1, SQLITE_ANY, 0,
                                DbHelper::directory_name_xFun, 0, 0);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot create function ``directory_name''"));
  }

  res = sqlite3_create_function(m_db, "is_dir_prefix", 2, SQLITE_ANY, 0,
                                DbHelper::is_dir_prefix_xFun, 0, 0);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot create function ``is_dir_prefix''"));
  }

  sqlite3_exec(m_db, INIT_DATABASE.c_str(), NULL, NULL, NULL);
  _LOG_DEBUG_COND(sqlite3_errcode(m_db) != SQLITE_OK, sqlite3_errmsg(m_db));
}

DbHelper::~DbHelper()
{
  int res = sqlite3_close(m_db);
  if (res != SQLITE_OK) {
    // complain
  }
}

void
DbHelper::hash_xStep(sqlite3_context* context, int argc, sqlite3_value** argv)
{
  if (argc != 2) {
    // _LOG_ERROR("Wrong arguments are supplied for ``hash'' function");
    sqlite3_result_error(context, "Wrong arguments are supplied for ``hash'' function", -1);
    return;
  }

  if (sqlite3_value_type(argv[0]) != SQLITE_BLOB || sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
    // _LOG_ERROR("Hash expects(blob,integer) parameters");
    sqlite3_result_error(context, "Hash expects(blob,integer) parameters", -1);
    return;
  }

  Sha256** digest = reinterpret_cast<Sha256**>(sqlite3_aggregate_context(context, sizeof(Sha256*)));

  if (digest == nullptr) {
    sqlite3_result_error_nomem(context);
    return;
  }

  if (*digest == nullptr) {
    *digest = new Sha256();
  }

  int nameBytes = sqlite3_value_bytes(argv[0]);
  const void* name = sqlite3_value_blob(argv[0]);
  sqlite3_int64 seqno = sqlite3_value_int64(argv[1]);

  (*digest)->update(reinterpret_cast<const uint8_t*>(name), nameBytes);
  (*digest)->update(reinterpret_cast<const uint8_t*>(&seqno), sizeof(sqlite3_int64));
}

void
DbHelper::hash_xFinal(sqlite3_context* context)
{
  Sha256** digest = reinterpret_cast<Sha256**>(sqlite3_aggregate_context(context, sizeof(Sha256*)));

  if (digest == nullptr) {
    sqlite3_result_error_nomem(context);
    return;
  }

  if (*digest == nullptr) {
    char charNullResult = 0;
    sqlite3_result_blob(context, &charNullResult, 1, SQLITE_TRANSIENT);
    return;
  }

  shared_ptr<const Buffer> hash = (*digest)->computeDigest();
  sqlite3_result_blob(context, hash->buf(), hash->size(), SQLITE_TRANSIENT);

  delete *digest;
}

void
DbHelper::is_prefix_xFun(sqlite3_context* context, int argc, sqlite3_value** argv)
{
  int len1 = sqlite3_value_bytes(argv[0]);
  int len2 = sqlite3_value_bytes(argv[1]);

  if (len1 == 0) {
    sqlite3_result_int(context, 1);
    return;
  }

  if (len1 > len2) // first parameter should be at most equal in length to the second one
  {
    sqlite3_result_int(context, 0);
    return;
  }

  if (memcmp(sqlite3_value_blob(argv[0]), sqlite3_value_blob(argv[1]), len1) == 0) {
    sqlite3_result_int(context, 1);
  }
  else {
    sqlite3_result_int(context, 0);
  }
}

void
DbHelper::directory_name_xFun(sqlite3_context* context, int argc, sqlite3_value** argv)
{
  if (argc != 1) {
    sqlite3_result_error(context, "``directory_name'' expects 1 text argument", -1);
    sqlite3_result_null(context);
    return;
  }

  if (sqlite3_value_bytes(argv[0]) == 0) {
    sqlite3_result_null(context);
    return;
  }

  boost::filesystem::path filePath(
    std::string(reinterpret_cast<const char*>(sqlite3_value_text(argv[0])),
                sqlite3_value_bytes(argv[0])));
  std::string dirPath = filePath.parent_path().generic_string();
  // _LOG_DEBUG("directory_name FUN: " << dirPath);
  if (dirPath.size() == 0) {
    sqlite3_result_null(context);
  }
  else {
    sqlite3_result_text(context, dirPath.c_str(), dirPath.size(), SQLITE_TRANSIENT);
  }
}

void
DbHelper::is_dir_prefix_xFun(sqlite3_context* context, int argc, sqlite3_value** argv)
{
  int len1 = sqlite3_value_bytes(argv[0]);
  int len2 = sqlite3_value_bytes(argv[1]);

  if (len1 == 0) {
    sqlite3_result_int(context, 1);
    return;
  }

  if (len1 > len2) // first parameter should be at most equal in length to the second one
  {
    sqlite3_result_int(context, 0);
    return;
  }

  if (memcmp(sqlite3_value_blob(argv[0]), sqlite3_value_blob(argv[1]), len1) == 0) {
    if (len1 == len2) {
      sqlite3_result_int(context, 1);
    }
    else {
      if (reinterpret_cast<const char*>(sqlite3_value_blob(argv[1]))[len1] == '/') {
        sqlite3_result_int(context, 1);
      }
      else {
        sqlite3_result_int(context, 0);
      }
    }
  }
  else {
    sqlite3_result_int(context, 0);
  }
}

} // chronoshare
} // ndn
