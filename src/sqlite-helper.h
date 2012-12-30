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
#include "hash-string-converter.h"

// temporarily ignoring all potential database errors

class DbHelper
{
public:
  DbHelper (const std::string &path);
  ~DbHelper ();

  // done
  void
  UpdateDeviceSeqno (const std::string &name, uint64_t seqNo);

  // std::string
  // LookupForwardingAlias (const std::string &deviceName);

  // void
  // UpdateForwardingAlias ();

  // done
  /**
   * Create an entry in SyncLog and SyncStateNodes corresponding to the current state of SyncNodes
   */
  HashPtr
  RememberStateInStateLog ();

  // done
  sqlite3_int64
  LookupSyncLog (const std::string &stateHash);

  // done
  sqlite3_int64
  LookupSyncLog (const Hash &stateHash);

  // How difference is exposed will be determined later by the actual protocol
  void
  FindStateDifferences (const std::string &oldHash, const std::string &newHash);

  void
  FindStateDifferences (const Hash &oldHash, const Hash &newHash);
  
private:
  static void
  hash_xStep (sqlite3_context *context, int argc, sqlite3_value **argv);

  static void
  hash_xFinal (sqlite3_context *context);

  static void
  hash2str_Func (sqlite3_context *context, int argc, sqlite3_value **argv);

  static void
  str2hash_Func (sqlite3_context *context, int argc, sqlite3_value **argv);
  
private:
  sqlite3 *m_db;
};

namespace Error {
struct Db : virtual boost::exception, virtual std::exception { };
}


#endif // SQLITE_HELPER_H
