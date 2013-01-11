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

#include "sync-core.h"

SyncCore::SyncCore
         : m_log(path, userName)
         , m_localPrefix(localPrefix)
         , m_syncPrefix(syncPrefix)
         , m_stateMsgCallback(callback)
         , m_handle(handle)
         , m_scheduler(scheduler)
{
  m_rootHash = m_log.RememberStateInStateLog();
  m_interestClosure = new Closure(0, boost::bind(&SyncCore::handleSyncData, this, _1, _2), boost::bind(&SyncCore::handleSyncInterestTimeout, this, _1));
  m_scheduler->start();
  sendSyncInterest();
}

SyncCore::~SyncCore()
{
  m_scheduler->stop();
  delete m_interestClosure;
  m_interestClosure = 0;
}

void
SyncCore::updateLocalPrefix(const Name &localPrefix)
{
  m_localPrefix = localPrefix;
  // optionally, we can have a sync action to announce the new prefix
  // we are not doing this for now
}

void
SyncCore::updateLocalState(seqno_t seqno)
{
  m_log.UpdateDeviceSeqNo(seqno);
  HashPtr oldHash = m_rootHash;
  m_rootHash = m_log.RememberStateInStateLog();

  SyncStateMsgPtr msg = m_log.FindStateDifferences(oldHash, m_rootHash);

  // reply sync Interest with oldHash as last component
  Name syncName = constructSyncName(oldHash);
  Bytes syncData;
  msgToBytes(msg, syncData);
  m_handle->publishData(syncName, syncData, FRESHNESS);

  // no hurry in sending out new Sync Interest; if others send the new Sync Interest first, no problem, we know the new root hash already;
  // this is trying to avoid the situation that the order of SyncData and new Sync Interest gets reversed at receivers
  ostringstream ss;
  ss << m_rootHash;
  TaskPtr task(new OneTimeTask(boost::bind(&SyncCore::sendSyncInterest, this), ss.str(), m_scheduler, 0.05));
  m_scheduler->addTask(task);
}

void
SyncCore::handleSyncInterest(const Name &name)
{
}

Closure::TimeoutCallbackReturnValue
SyncCore::handleSyncInterestTimeout(const Name &name)
{
  // sendInterestInterest with the current root hash;
  sendSyncInterest();
  return Closure::OK;
}

void
SyncCore::handleSyncData(const Name &name, const Bytes &content)
{
  SyncStateMsgPtr msg(new SyncStateMsg);
  bool success = msg->ParseFromArray(head(content), content.size());
  if(!success)
  {
    // ignore misformed SyncData
    cerr << "Misformed SyncData with name: " << name << endl;
    return;
  }

  int size = msg->state_size();
  int index = 0;
  while (index < size)
  {
    SyncState state = msg->state(index);
    index++;
  }

}

void
SyncCore::sendSyncInterest()
{
  Name syncInterest = constructSyncName(m_rootHash);
  sendInterest(syncInterest, m_interestClosure);
}

Name
SyncCore::constructSyncName(const HashPtr &hash)
{
  Bytes bytes;
  readRaw(bytes, (const unsigned char*)hash->GetHash(), hash->GetHashBytes);
  Name syncName = m_syncPrefix;
  syncName.append(bytes);
  return syncName;
}

void
SyncCore::msgToBytes(const SyncStateMsgPtr &msg, Bytes &bytes)
{
  int size = msg.ByteSize();
  bytes.resize(size);
  msg.SerializeToArray(head(bytes), size);
}
