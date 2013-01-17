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

#include "sync-core.h"

const string SyncCore::RECOVER = "RECOVER";
const double SyncCore::WAIT = 0.05;
const double SyncCore::RANDOM_PERCENT = 0.5;

SyncCore::SyncCore(const string &path, const Name &userName, const Name &localPrefix, const Name &syncPrefix, const StateMsgCallback &callback, CcnxWrapperPtr handle)
         : m_log(path, userName.toString())
         , m_scheduler(new Scheduler())
         , m_stateMsgCallback(callback)
         , m_userName(userName)
         , m_localPrefix(localPrefix)
         , m_syncPrefix(syncPrefix)
         , m_handle(handle)
         , m_recoverWaitGenerator(new RandomIntervalGenerator(WAIT, RANDOM_PERCENT, RandomIntervalGenerator::UP))
{
  m_rootHash = m_log.RememberStateInStateLog();
  m_syncClosure = new Closure(0, boost::bind(&SyncCore::handleSyncData, this, _1, _2), boost::bind(&SyncCore::handleSyncInterestTimeout, this, _1));
  m_recoverClosure = new Closure(0, boost::bind(&SyncCore::handleRecoverData, this, _1, _2), boost::bind(&SyncCore::handleRecoverInterestTimeout, this, _1));
  m_handle->setInterestFilter(m_syncPrefix, boost::bind(&SyncCore::handleInterest, this, _1));
  m_log.initYP(m_yp);
  m_scheduler->start();
  sendSyncInterest();
}

SyncCore::~SyncCore()
{
  m_scheduler->shutdown();
  if (m_syncClosure != 0)
  {
    delete m_syncClosure;
    m_syncClosure = 0;
  }

  if (m_recoverClosure != 0)
  {
    delete m_recoverClosure;
    m_recoverClosure = 0;
  }
}

Name
SyncCore::yp(const Name &deviceName)
{
  Name locator;
  ReadLock lock(m_ypMutex);
  if (m_yp.find(deviceName) != m_yp.end())
  {
    locator = m_yp[deviceName];
  }
  return locator;
}

void
SyncCore::updateLocalPrefix(const Name &localPrefix)
{
  m_localPrefix = localPrefix;
  // optionally, we can have a sync action to announce the new prefix
  // we are not doing this for now
}

void
SyncCore::updateLocalState(sqlite3_int64 seqno)
{
  m_log.UpdateDeviceSeqNo(m_userName, seqno);
  // choose to update locator everytime
  m_log.UpdateLocator(m_userName, m_localPrefix);
  HashPtr oldHash = m_rootHash;
  m_rootHash = m_log.RememberStateInStateLog();

  SyncStateMsgPtr msg = m_log.FindStateDifferences(*oldHash, *m_rootHash);

  // reply sync Interest with oldHash as last component
  Name syncName = constructSyncName(oldHash);
  Bytes syncData;
  msgToBytes(msg, syncData);
  m_handle->publishData(syncName, syncData, FRESHNESS);
  cout << m_userName << " publishes: " << *oldHash << endl;

  // no hurry in sending out new Sync Interest; if others send the new Sync Interest first, no problem, we know the new root hash already;
  // this is trying to avoid the situation that the order of SyncData and new Sync Interest gets reversed at receivers
  ostringstream ss;
  ss << *m_rootHash;
  TaskPtr task(new OneTimeTask(boost::bind(&SyncCore::sendSyncInterest, this), ss.str(), m_scheduler, 0.1));
  m_scheduler->addTask(task);
  sendSyncInterest();
}

void
SyncCore::handleInterest(const Name &name)
{
  int size = name.size();
  int prefixSize = m_syncPrefix.size();
  if (size == prefixSize + 1)
  {
    // this is normal sync interest
    handleSyncInterest(name);
  }
  else if (size == prefixSize + 2 && name.getCompAsString(m_syncPrefix.size()) == RECOVER)
  {
    // this is recovery interest
    handleRecoverInterest(name);
  }
}

void
SyncCore::handleRecoverInterest(const Name &name)
{
  Bytes hashBytes = name.getComp(name.size() - 1);
  // this is the hash unkonwn to the sender of the interest
  Hash hash(head(hashBytes), hashBytes.size());
  if (m_log.LookupSyncLog(hash) > 0)
  {
    // we know the hash, should reply everything
    SyncStateMsgPtr msg = m_log.FindStateDifferences(*(Hash::Origin), *m_rootHash);
    // DEBUG
    /*
    assert(msg->state_size() > 0);
    int size = msg->state_size();
    int index = 0;
    cout << "Reply recover interest with: " << endl;
    while (index < size)
    {
      SyncState state = msg->state(index);
      string strName = state.name();
      string strLoc = state.locator();
      cout << "Name: " << Name((const unsigned char *)strName.c_str(), strName.size()) << ", Loc: " << Name((const unsigned char *)strLoc.c_str(), strLoc.size()) << ", seq: " << state.seq() << endl;
      if (state.type() == SyncState::UPDATE)
      {
        cout << "Action: update" << endl;
      }
      else
      {
        cout << "Action: delete" << endl;
      }
      index++;
    }
    */
    // END DEBUG
    Bytes syncData;
    msgToBytes(msg, syncData);
    m_handle->publishData(name, syncData, FRESHNESS);
    cout << m_userName << " publishes " << hash << endl;
  }
  else
  {
    // we don't recognize this hash, can not help
  }
}

void
SyncCore::handleSyncInterest(const Name &name)
{
  Bytes hashBytes = name.getComp(name.size() - 1);
  HashPtr hash(new Hash(head(hashBytes), hashBytes.size()));
  if (*hash == *m_rootHash)
  {
    // we have the same hash; nothing needs to be done
    cout << "same as root hash: " << *hash << endl;
    return;
  }
  else if (m_log.LookupSyncLog(*hash) > 0)
  {
    // we know something more
    cout << "found hash in sync log" << endl;
    SyncStateMsgPtr msg = m_log.FindStateDifferences(*hash, *m_rootHash);

    Bytes syncData;
    msgToBytes(msg, syncData);
    m_handle->publishData(name, syncData, FRESHNESS);
  }
  else
  {
    // we don't recognize the hash, send recover Interest if still don't know the hash after a randomized wait period
    ostringstream ss;
    ss << *hash;
    double wait = m_recoverWaitGenerator->nextInterval();
    cout << m_userName << ", rootHash: " << *m_rootHash << ", hash: " << *hash << endl;
    cout << "recover task scheduled after wait: " << wait << endl;
    TaskPtr task(new OneTimeTask(boost::bind(&SyncCore::recover, this, hash), ss.str(), m_scheduler, wait));
    m_scheduler->addTask(task);

  }
}

Closure::TimeoutCallbackReturnValue
SyncCore::handleSyncInterestTimeout(const Name &name)
{
  // sendInterestInterest with the current root hash;
  sendSyncInterest();
  return Closure::RESULT_OK;
}

Closure::TimeoutCallbackReturnValue
SyncCore::handleRecoverInterestTimeout(const Name &name)
{
  // We do not re-express recovery interest for now
  // if difference is not resolved, the sync interest will trigger
  // recovery anyway; so it's not so important to have recovery interest
  // re-expressed
  return Closure::RESULT_OK;
}

void
SyncCore::handleRecoverData(const Name &name, const Bytes &content)
{
  //cout << "handle recover data" << endl;
  handleStateData(content);
  sendSyncInterest();
}

void
SyncCore::handleSyncData(const Name &name, const Bytes &content)
{
  // suppress recover in interest - data out of order case
  handleStateData(content);

  // resume outstanding sync interest
  sendSyncInterest();
}

void
SyncCore::handleStateData(const Bytes &content)
{
  SyncStateMsgPtr msg(new SyncStateMsg);
  bool success = msg->ParseFromArray(head(content), content.size());
  if(!success)
  {
    // ignore misformed SyncData
    cerr << "Misformed SyncData"<< endl;
    return;
  }

  int size = msg->state_size();
  int index = 0;
  while (index < size)
  {
    SyncState state = msg->state(index);
    string devStr = state.name();
    Name deviceName((const unsigned char *)devStr.c_str(), devStr.size());
  //  cout << "Got Name: " << deviceName;
    if (state.type() == SyncState::UPDATE)
    {
      sqlite3_int64 seqno = state.seq();
   //   cout << ", Got seq: " << seqno << endl;
      m_log.UpdateDeviceSeqNo(deviceName, seqno);
      if (state.has_locator())
      {
        string locStr = state.locator();
        Name locatorName((const unsigned char *)locStr.c_str(), locStr.size());
    //    cout << ", Got loc: " << locatorName << endl;
        m_log.UpdateLocator(deviceName, locatorName);
        WriteLock lock(m_ypMutex);
        m_yp[deviceName] = locatorName;
      }
    }
    else
    {
      cout << "nani" << endl;
      deregister(deviceName);
    }
    index++;
  }

  // find the actuall difference and invoke callback on the actual difference
  HashPtr oldHash = m_rootHash;
  m_rootHash = m_log.RememberStateInStateLog();
  SyncStateMsgPtr diff = m_log.FindStateDifferences(*oldHash, *m_rootHash);

  m_stateMsgCallback(diff);
}

void
SyncCore::sendSyncInterest()
{
  Name syncInterest = constructSyncName(m_rootHash);
  m_handle->sendInterest(syncInterest, m_syncClosure);
  cout << m_userName << " send SYNC interest: " << *m_rootHash << endl;
}

void
SyncCore::recover(const HashPtr &hash)
{
  if (!(*hash == *m_rootHash) && m_log.LookupSyncLog(*hash) <= 0)
  {
    cout << m_userName << ", Recover for: " << *hash << endl;
    // unfortunately we still don't recognize this hash
    Bytes bytes;
    readRaw(bytes, (const unsigned char *)hash->GetHash(), hash->GetHashBytes());
    Name recoverInterest = m_syncPrefix;
    recoverInterest.appendComp(RECOVER);
    // append the unknown hash
    recoverInterest.appendComp(bytes);
    m_handle->sendInterest(recoverInterest, m_recoverClosure);
    cout << m_userName << " send RECOVER Interest: " << *hash << endl;
  }
  else
  {
    // we already learned the hash; cheers!
  }
}

void
SyncCore::deregister(const Name &name)
{
  // Do nothing for now
  // TODO: handle deregistering
}

Name
SyncCore::constructSyncName(const HashPtr &hash)
{
  Bytes bytes;
  readRaw(bytes, (const unsigned char*)hash->GetHash(), hash->GetHashBytes());
  Name syncName = m_syncPrefix;
  syncName.appendComp(bytes);
  return syncName;
}

void
SyncCore::msgToBytes(const SyncStateMsgPtr &msg, Bytes &bytes)
{
  int size = msg->ByteSize();
  bytes.resize(size);
  msg->SerializeToArray(head(bytes), size);
}
