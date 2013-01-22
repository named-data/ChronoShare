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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "sync-log.h"
#include "ccnx-wrapper.h"
#include "scheduler.h"

#include <boost/function.hpp>

class SyncCore
{
public:
  typedef boost::function<void (const SyncStateMsgPtr & stateMsg) > StateMsgCallback;
  // typedef map<Name, Name> YellowPage;
  typedef boost::shared_mutex Mutex;
  typedef boost::shared_lock<Mutex> ReadLock;
  typedef boost::unique_lock<Mutex> WriteLock;

  static const int FRESHNESS = 2; // seconds
  static const string RECOVER;
  static const double WAIT; // seconds;
  static const double RANDOM_PERCENT; // seconds;

public:
  SyncCore(SyncLogPtr syncLog
           , const Ccnx::Name &userName
           , const Ccnx::Name &localPrefix      // routable name used by the local user
           , const Ccnx::Name &syncPrefix       // the prefix for the sync collection
           , const StateMsgCallback &callback   // callback when state change is detected
           , Ccnx::CcnxWrapperPtr ccnx
           , SchedulerPtr scheduler);
  ~SyncCore();

  void
  updateLocalState (sqlite3_int64);

// ------------------ only used in test -------------------------
public:
  HashPtr
  root() const { return m_rootHash; }

  sqlite3_int64
  seq (const Ccnx::Name &name);

private:
  void
  handleInterest(const Ccnx::Name &name);

  void
  handleSyncData(const Ccnx::Name &name, Ccnx::PcoPtr content);

  void
  handleRecoverData(const Ccnx::Name &name, Ccnx::PcoPtr content);

  Ccnx::Closure::TimeoutCallbackReturnValue
  handleSyncInterestTimeout(const Ccnx::Name &name);

  Ccnx::Closure::TimeoutCallbackReturnValue
  handleRecoverInterestTimeout(const Ccnx::Name &name);

  void
  deregister(const Ccnx::Name &name);

  void
  recover(const HashPtr &hash);

private:
  void
  sendSyncInterest();

  void
  handleSyncInterest(const Ccnx::Name &name);

  void
  handleRecoverInterest(const Ccnx::Name &name);

  void
  handleStateData(const Ccnx::Bytes &content);

private:
  Ccnx::CcnxWrapperPtr m_ccnx;

  SyncLogPtr m_log;
  SchedulerPtr m_scheduler;
  StateMsgCallback m_stateMsgCallback;

  Ccnx::Name m_syncPrefix;
  HashPtr m_rootHash;

  IntervalGeneratorPtr m_recoverWaitGenerator;
};

#endif // SYNC_CORE_H
