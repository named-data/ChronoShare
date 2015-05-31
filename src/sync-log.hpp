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

#ifndef CHRONOSHARE_SRC_SYNC_LOG_HPP
#define CHRONOSHARE_SRC_SYNC_LOG_HPP

#include "db-helper.hpp"
#include "sync-state.pb.h"
#include "core/chronoshare-common.hpp"

#include <ndn-cxx/name.hpp>

#include <map>

// @todo Replace with std::thread
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ndn {
namespace chronoshare {

typedef shared_ptr<SyncStateMsg> SyncStateMsgPtr;

class SyncLog : public DbHelper
{
public:
  class Error : public DbHelper::Error
  {
  public:
    explicit
    Error(const std::string& what)
      : DbHelper::Error(what)
    {
    }
  };

  SyncLog(const boost::filesystem::path& path, const Name& localName);

  /**
   * @brief Get local username
   */
  const Name&
  GetLocalName() const;

  sqlite3_int64
  GetNextLocalSeqNo(); // side effect: local seq_no will be increased

  // done
  void
  UpdateDeviceSeqNo(const Name& name, sqlite3_int64 seqNo);

  void
  UpdateLocalSeqNo(sqlite3_int64 seqNo);

  Name
  LookupLocator(const Name& deviceName);

  Name
  LookupLocalLocator();

  void
  UpdateLocator(const Name& deviceName, const Name& locator);

  void
  UpdateLocalLocator(const Name& locator);

  // done
  /**
   * Create an 1ntry in SyncLog and SyncStateNodes corresponding to the current state of SyncNodes
   */
  ConstBufferPtr
  RememberStateInStateLog();

  // done
  sqlite3_int64
  LookupSyncLog(const std::string& stateHash);

  // done
  sqlite3_int64
  LookupSyncLog(const Buffer& stateHash);

  // How difference is exposed will be determined later by the actual protocol
  SyncStateMsgPtr
  FindStateDifferences(const std::string& oldHash, const std::string& newHash,
                       bool includeOldSeq = false);

  SyncStateMsgPtr
  FindStateDifferences(const Buffer& oldHash, const Buffer& newHash, bool includeOldSeq = false);

  //-------- only used in test -----------------
  sqlite3_int64
  SeqNo(const Name& name);

  sqlite3_int64
  LogSize();

protected:
  void
  UpdateDeviceSeqNo(sqlite3_int64 deviceId, sqlite3_int64 seqNo);

protected:
  Name m_localName;

  sqlite3_int64 m_localDeviceId;

  typedef boost::mutex Mutex;
  typedef boost::unique_lock<Mutex> WriteLock;

  Mutex m_stateUpdateMutex;
};

typedef shared_ptr<SyncLog> SyncLogPtr;

inline const Name&
SyncLog::GetLocalName() const
{
  return m_localName;
}

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_SYNC_LOG_HPP
