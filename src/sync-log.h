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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef SYNC_LOG_H
#define SYNC_LOG_H

#include "db-helper.h"
#include <sync-state.pb.h>
#include <ccnx-name.h>
#include <map>
#include <boost/thread/shared_mutex.hpp>

typedef boost::shared_ptr<SyncStateMsg> SyncStateMsgPtr;

class SyncLog : public DbHelper
{
public:
  SyncLog (const boost::filesystem::path &path, const Ccnx::Name &localName);

  /**
   * @brief Get local username
   */
  inline const Ccnx::Name &
  GetLocalName () const;

  sqlite3_int64
  GetNextLocalSeqNo (); // side effect: local seq_no will be increased

  // done
  void
  UpdateDeviceSeqNo (const Ccnx::Name &name, sqlite3_int64 seqNo);

  void
  UpdateLocalSeqNo (sqlite3_int64 seqNo);

  Ccnx::Name
  LookupLocator (const Ccnx::Name &deviceName);

  Ccnx::Name
  LookupLocalLocator ();

  void
  UpdateLocator (const Ccnx::Name &deviceName, const Ccnx::Name &locator);

  void
  UpdateLocalLocator (const Ccnx::Name &locator);

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
  SyncStateMsgPtr
  FindStateDifferences (const std::string &oldHash, const std::string &newHash, bool includeOldSeq = false);

  SyncStateMsgPtr
  FindStateDifferences (const Hash &oldHash, const Hash &newHash, bool includeOldSeq = false);

  //-------- only used in test -----------------
  sqlite3_int64
  SeqNo(const Ccnx::Name &name);

  sqlite3_int64
  LogSize ();

protected:
  void
  UpdateDeviceSeqNo (sqlite3_int64 deviceId, sqlite3_int64 seqNo);

protected:
  Ccnx::Name m_localName;

  sqlite3_int64 m_localDeviceId;

  typedef boost::mutex Mutex;
  typedef boost::unique_lock<Mutex> WriteLock;

  Mutex m_stateUpdateMutex;
};

typedef boost::shared_ptr<SyncLog> SyncLogPtr;

const Ccnx::Name &
SyncLog::GetLocalName () const
{
  return m_localName;
}

#endif // SYNC_LOG_H
