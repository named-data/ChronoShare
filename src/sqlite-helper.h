/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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

#ifndef SQLITE_HELPER_H
#define SQLITE_HELPER_H

#include <sqlite3.h>
#include <openssl/evp.h>
#include <boost/exception/all.hpp>
#include <string>

// temporarily ignoring all potential database errors

class DbHelper
{
public:
  DbHelper (const std::string &path);
  ~DbHelper ();

  
  
private:
  static void
  hash_xStep (sqlite3_context*,int,sqlite3_value**);

  static void
  hash_xFinal (sqlite3_context*);
  
private:
  sqlite3 *m_db;
};

namespace Error {
struct Db : virtual boost::exception, virtual std::exception { };
}


#endif // SQLITE_HELPER_H
