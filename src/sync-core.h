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
#include <ccnx-wrapper.h>
#include <event-scheduler.h>
#include <boost/function.hpp>
#include <boost/thread/shared_mutex.hpp>

using namespace std;
using namespace Ccnx;

class SyncCore
{
public:
  typedef boost::function<void (const SyncStateMsgPtr & stateMsg) > StateMsgCallback;
  typedef sqlite3_int64 seqno_t;
  typedef Map<Name, Name> YellowPage;
  typedef boost::shared_mutex Mutex;
  typedef boost::shared_lock<Mutex> ReadLock;
  typedef boost::unique_lock<Mutex> WriteLock;

  static const int FRESHNESS = 2;

public:
  SyncCore(const string &path                   // path where SyncLog is stored
           , const Name &userName               // unique permanent name for local user
           , const Name &localPrefix            // routable name used by the local user
           , const Name &syncPrefix             // the prefix for the sync collection
           , const StateMsgCallback &callback   // callback when state change is detected
           , CcnxWrapperPtr handle
           , SchedulerPtr scheduler);
  ~SyncCore();

  // some other code should call this fuction when local prefix
  // changes; e.g. when wake up in another network
  void
  updateLocalPrefix(const Name &localPrefix);

  void
  updateLocalState(seqno_t);

  Name
  yp(const Name &name);

  void
  handleSyncInterest(const Name &name);

  Closure::TimeoutCallbackReturnValue
  handleSyncInterestTimeout(const Name &name);

  void
  handleSyncData(const Name &name, const Bytes &content);

  void
  deregister();

protected:
  void
  sendSyncInterest();

  Name
  constructSyncName(const HashPtr &hash);

  static void
  msgToBytes(const SyncStateMsgPtr &msg, Bytes &bytes);

protected:
  SyncLog m_log;
  SchedulerPtr m_scheduler;
  StateMsgCallback m_stateMsgCallback;
  Name m_userName;
  Name m_localPrefix;
  Name m_syncPrefix;
  HashPtr m_rootHash;
  YellowPage m_yp;
  Mutex m_ypMutex;
  CcnxWrapperPtr m_handle;
  Closure *m_interestClosure;

};

#endif // SYNC_CORE_H
