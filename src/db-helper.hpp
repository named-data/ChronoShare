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

#ifndef DB_HELPER_H
#define DB_HELPER_H

#include "hash-helper.hpp"
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <openssl/evp.h>
#include <sqlite3.h>
#include <stdint.h>
#include <string>

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;

class DbHelper
{
public:
  DbHelper(const boost::filesystem::path& path, const std::string& dbname);
  virtual ~DbHelper();

private:
  static void
  hash_xStep(sqlite3_context* context, int argc, sqlite3_value** argv);

  static void
  hash_xFinal(sqlite3_context* context);

  static void
  is_prefix_xFun(sqlite3_context* context, int argc, sqlite3_value** argv);

  static void
  directory_name_xFun(sqlite3_context* context, int argc, sqlite3_value** argv);

  static void
  is_dir_prefix_xFun(sqlite3_context* context, int argc, sqlite3_value** argv);

protected:
  sqlite3* m_db;
};

namespace Error {
struct Db : virtual boost::exception, virtual std::exception
{
};
}

typedef boost::shared_ptr<DbHelper> DbHelperPtr;


#endif // DB_HELPER_H
