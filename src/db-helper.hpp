/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef DB_HELPER_H
#define DB_HELPER_H

#include <stdint.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <boost/exception/all.hpp>
#include <string>
#include "hash-helper.h"
#include <boost/filesystem.hpp>

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;

class DbHelper
{
public:
  DbHelper (const boost::filesystem::path &path, const std::string &dbname);
  virtual ~DbHelper ();

private:
  static void
  hash_xStep (sqlite3_context *context, int argc, sqlite3_value **argv);

  static void
  hash_xFinal (sqlite3_context *context);

  static void
  is_prefix_xFun (sqlite3_context *context, int argc, sqlite3_value **argv);

  static void
  directory_name_xFun (sqlite3_context *context, int argc, sqlite3_value **argv);

  static void
  is_dir_prefix_xFun (sqlite3_context *context, int argc, sqlite3_value **argv);

protected:
  sqlite3 *m_db;
};

namespace Error {
struct Db : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<DbHelper> DbHelperPtr;


#endif // DB_HELPER_H
