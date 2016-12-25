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

#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "ccnx-selectors.hpp"
#include "ccnx-wrapper.hpp"
#include "scheduler.hpp"
#include "sync-log.hpp"
#include "task.hpp"

#include <boost/function.hpp>

class SyncCore
{
public:
  typedef boost::function<void(SyncStateMsgPtr stateMsg)> StateMsgCallback;

  static const int FRESHNESS = 2; // seconds
  static const string RECOVER;
  static const double WAIT;           // seconds;
  static const double RANDOM_PERCENT; // seconds;

public:
  SyncCore(SyncLogPtr syncLog, const Ccnx::Name& userName,
           const Ccnx::Name& localPrefix // routable name used by the local user
           ,
           const Ccnx::Name& syncPrefix // the prefix for the sync collection
           ,
           const StateMsgCallback& callback // callback when state change is detected
           ,
           Ccnx::CcnxWrapperPtr ccnx, double syncInterestInterval = -1.0);
  ~SyncCore();

  void
  localStateChanged();

  /**
   * @brief Schedule an event to update local state with a small delay
   *
   * This call is preferred to localStateChanged if many local state updates
   * are anticipated within a short period of time
   */
  void
  localStateChangedDelayed();

  void updateLocalState(sqlite3_int64);

  // ------------------ only used in test -------------------------
public:
  HashPtr
  root() const
  {
    return m_rootHash;
  }

  sqlite3_int64
  seq(const Ccnx::Name& name);

private:
  void
  handleInterest(const Ccnx::Name& name);

  void
  handleSyncData(const Ccnx::Name& name, Ccnx::PcoPtr content);

  void
  handleRecoverData(const Ccnx::Name& name, Ccnx::PcoPtr content);

  void
  handleSyncInterestTimeout(const Ccnx::Name& name, const Ccnx::Closure& closure,
                            Ccnx::Selectors selectors);

  void
  handleRecoverInterestTimeout(const Ccnx::Name& name, const Ccnx::Closure& closure,
                               Ccnx::Selectors selectors);

  void
  deregister(const Ccnx::Name& name);

  void
  recover(HashPtr hash);

private:
  void
  sendSyncInterest();

  void
  handleSyncInterest(const Ccnx::Name& name);

  void
  handleRecoverInterest(const Ccnx::Name& name);

  void
  handleStateData(const Ccnx::Bytes& content);

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
