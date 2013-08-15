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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "sync-log.h"
#include "ndnx-wrapper.h"
#include "ndnx-selectors.h"
#include "scheduler.h"
#include "task.h"

#include <boost/function.hpp>

class SyncCore
{
public:
  typedef boost::function<void (SyncStateMsgPtr stateMsg) > StateMsgCallback;

  static const int FRESHNESS = 2; // seconds
  static const string RECOVER;
  static const double WAIT; // seconds;
  static const double RANDOM_PERCENT; // seconds;

public:
  SyncCore(SyncLogPtr syncLog
           , const Ndnx::Name &userName
           , const Ndnx::Name &localPrefix      // routable name used by the local user
           , const Ndnx::Name &syncPrefix       // the prefix for the sync collection
           , const StateMsgCallback &callback   // callback when state change is detected
           , Ndnx::NdnxWrapperPtr ndnx
           , double syncInterestInterval = -1.0);
  ~SyncCore();

  void
  localStateChanged ();

  /**
   * @brief Schedule an event to update local state with a small delay
   *
   * This call is preferred to localStateChanged if many local state updates
   * are anticipated within a short period of time
   */
  void
  localStateChangedDelayed ();

  void
  updateLocalState (sqlite3_int64);

// ------------------ only used in test -------------------------
public:
  HashPtr
  root() const { return m_rootHash; }

  sqlite3_int64
  seq (const Ndnx::Name &name);

private:
  void
  handleInterest(const Ndnx::Name &name);

  void
  handleSyncData(const Ndnx::Name &name, Ndnx::PcoPtr content);

  void
  handleRecoverData(const Ndnx::Name &name, Ndnx::PcoPtr content);

  void
  handleSyncInterestTimeout(const Ndnx::Name &name, const Ndnx::Closure &closure, Ndnx::Selectors selectors);

  void
  handleRecoverInterestTimeout(const Ndnx::Name &name, const Ndnx::Closure &closure, Ndnx::Selectors selectors);

  void
  deregister(const Ndnx::Name &name);

  void
  recover(HashPtr hash);

private:
  void
  sendSyncInterest();

  void
  handleSyncInterest(const Ndnx::Name &name);

  void
  handleRecoverInterest(const Ndnx::Name &name);

  void
  handleStateData(const Ndnx::Bytes &content);

private:
  Ndnx::NdnxWrapperPtr m_ndnx;

  SyncLogPtr m_log;
  SchedulerPtr m_scheduler;
  StateMsgCallback m_stateMsgCallback;

  Ndnx::Name m_syncPrefix;
  HashPtr m_rootHash;

  IntervalGeneratorPtr m_recoverWaitGenerator;

  TaskPtr m_sendSyncInterestTask;

  double m_syncInterestInterval;
};

#endif // SYNC_CORE_H
