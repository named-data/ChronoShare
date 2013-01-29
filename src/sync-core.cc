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
#include "sync-state-helper.h"
#include "logging.h"
#include "random-interval-generator.h"
#include "simple-interval-generator.h"
#include "periodic-task.h"

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

INIT_LOGGER ("Sync.Core");

const string SyncCore::RECOVER = "RECOVER";
const double SyncCore::WAIT = 0.05;
const double SyncCore::RANDOM_PERCENT = 0.5;

const std::string SYNC_INTEREST_TAG = "send-sync-interest";
const std::string SYNC_INTEREST_TAG2 = "send-sync-interest2";

using namespace boost;
using namespace Ccnx;

SyncCore::SyncCore(SyncLogPtr syncLog, const Name &userName, const Name &localPrefix, const Name &syncPrefix,
                   const StateMsgCallback &callback, CcnxWrapperPtr ccnx, double syncInterestInterval/*= -1.0*/)
  : m_ccnx (ccnx)
  , m_log(syncLog)
  , m_scheduler(new Scheduler ())
  , m_stateMsgCallback(callback)
  , m_syncPrefix(syncPrefix)
  , m_recoverWaitGenerator(new RandomIntervalGenerator(WAIT, RANDOM_PERCENT, RandomIntervalGenerator::UP))
  , m_syncInterestInterval(syncInterestInterval)
{
  m_rootHash = m_log->RememberStateInStateLog();

  m_ccnx->setInterestFilter(m_syncPrefix, boost::bind(&SyncCore::handleInterest, this, _1));
  // m_log->initYP(m_yp);
  m_log->UpdateLocalLocator (localPrefix);

  m_scheduler->start();

  double interval = (m_syncInterestInterval > 0 && m_syncInterestInterval < 30.0) ? m_syncInterestInterval : 4.0;
  m_sendSyncInterestTask = make_shared<PeriodicTask>(bind(&SyncCore::sendSyncInterest, this), SYNC_INTEREST_TAG, m_scheduler, make_shared<SimpleIntervalGenerator>(interval));
  // sendSyncInterest();
  Scheduler::scheduleOneTimeTask (m_scheduler, 0.1, bind(&SyncCore::sendSyncInterest, this), SYNC_INTEREST_TAG2);
}

SyncCore::~SyncCore()
{
  // m_scheduler->shutdown ();
  // need to "deregister" closures
}

void
SyncCore::updateLocalPrefix (const Name &localPrefix)
{
  m_log->UpdateLocalLocator (localPrefix);
}

void
SyncCore::updateLocalState(sqlite3_int64 seqno)
{
  m_log->UpdateLocalSeqNo (seqno);
  localStateChanged();
}

void
SyncCore::localStateChanged()
{
  HashPtr oldHash = m_rootHash;
  m_rootHash = m_log->RememberStateInStateLog();

  SyncStateMsgPtr msg = m_log->FindStateDifferences(*oldHash, *m_rootHash);

  // reply sync Interest with oldHash as last component
  Name syncName = Name (m_syncPrefix)(oldHash->GetHash(), oldHash->GetHashBytes());
  BytesPtr syncData = serializeMsg (*msg);

  m_ccnx->publishData(syncName, *syncData, FRESHNESS);
  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] localStateChanged ");
  _LOG_TRACE ("[" << m_log->GetLocalName () << "] publishes: " << oldHash->shortHash ());
  // _LOG_TRACE (msg);

  m_scheduler->deleteTask (SYNC_INTEREST_TAG2);
  // no hurry in sending out new Sync Interest; if others send the new Sync Interest first, no problem, we know the new root hash already;
  // this is trying to avoid the situation that the order of SyncData and new Sync Interest gets reversed at receivers
  Scheduler::scheduleOneTimeTask (m_scheduler, 0.05,
                                  bind(&SyncCore::sendSyncInterest, this),
                                  SYNC_INTEREST_TAG2);

  //sendSyncInterest();
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
  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] <<<<< RECOVER Interest with name " << name);

  Bytes hashBytes = name.getComp(name.size() - 1);
  // this is the hash unkonwn to the sender of the interest
  Hash hash(head(hashBytes), hashBytes.size());
  if (m_log->LookupSyncLog(hash) > 0)
  {
    // we know the hash, should reply everything
    SyncStateMsgPtr msg = m_log->FindStateDifferences(*(Hash::Origin), *m_rootHash);

    BytesPtr syncData = serializeMsg (*msg);
    m_ccnx->publishData(name, *syncData, FRESHNESS);
    _LOG_TRACE ("[" << m_log->GetLocalName () << "] publishes " << hash.shortHash ());
    // _LOG_TRACE (msg);
  }
  else
    {
      // we don't recognize this hash, can not help
    }
}

void
SyncCore::handleSyncInterest(const Name &name)
{
  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] <<<<< SYNC Interest with name " << name);

  Bytes hashBytes = name.getComp(name.size() - 1);
  HashPtr hash(new Hash(head(hashBytes), hashBytes.size()));
  if (*hash == *m_rootHash)
  {
    // we have the same hash; nothing needs to be done
    _LOG_TRACE ("same as root hash: " << hash->shortHash ());
    return;
  }
  else if (m_log->LookupSyncLog(*hash) > 0)
  {
    // we know something more
    _LOG_TRACE ("found hash in sync log");
    SyncStateMsgPtr msg = m_log->FindStateDifferences(*hash, *m_rootHash);

    BytesPtr syncData = serializeMsg (*msg);
    m_ccnx->publishData(name, *syncData, FRESHNESS);
    _LOG_TRACE (m_log->GetLocalName () << " publishes: " << hash->shortHash ());
    _LOG_TRACE (msg);
  }
  else
  {
    // we don't recognize the hash, send recover Interest if still don't know the hash after a randomized wait period
    double wait = m_recoverWaitGenerator->nextInterval();
    _LOG_TRACE (m_log->GetLocalName () << ", rootHash: " << *m_rootHash << ", hash: " << hash->shortHash ());
    _LOG_TRACE ("recover task scheduled after wait: " << wait);

    Scheduler::scheduleOneTimeTask (m_scheduler,
                                    wait, boost::bind(&SyncCore::recover, this, hash),
                                    "r-"+lexical_cast<string> (*hash));
  }
}

void
SyncCore::handleSyncInterestTimeout(const Name &name, const Closure &closure, Selectors selectors)
{
  // sync interest will be resent by scheduler
}

void
SyncCore::handleRecoverInterestTimeout(const Name &name, const Closure &closure, Selectors selectors)
{
  // We do not re-express recovery interest for now
  // if difference is not resolved, the sync interest will trigger
  // recovery anyway; so it's not so important to have recovery interest
  // re-expressed
}

void
SyncCore::handleRecoverData(const Name &name, PcoPtr content)
{
  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] <<<<< RECOVER DATA with name: " << name);
  //cout << "handle recover data" << end;
  if (content && content->contentPtr () && content->contentPtr ()->size () > 0)
    {
      handleStateData(*content->contentPtr ());
    }
  else
    {
      _LOG_ERROR ("Got recovery DATA with empty content");
    }

  // sendSyncInterest();
  m_scheduler->deleteTask (SYNC_INTEREST_TAG2);
  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind(&SyncCore::sendSyncInterest, this),
                                  SYNC_INTEREST_TAG2);
}

void
SyncCore::handleSyncData(const Name &name, PcoPtr content)
{
  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] <<<<< SYNC DATA with name: " << name);

  // suppress recover in interest - data out of order case
  if (content && content->contentPtr () && content->contentPtr ()->size () > 0)
    {
      handleStateData(*content->contentPtr ());
    }
  else
    {
      _LOG_ERROR ("Got sync DATA with empty content");
    }

  // resume outstanding sync interest
  // sendSyncInterest();

  m_scheduler->deleteTask (SYNC_INTEREST_TAG2);
  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind(&SyncCore::sendSyncInterest, this),
                                  SYNC_INTEREST_TAG2);
}

void
SyncCore::handleStateData(const Bytes &content)
{
  SyncStateMsgPtr msg(new SyncStateMsg);
  bool success = msg->ParseFromArray(head(content), content.size());
  if(!success)
  {
    // ignore misformed SyncData
    _LOG_ERROR ("Misformed SyncData");
    return;
  }

  _LOG_TRACE (m_log->GetLocalName () << " receives Msg ");
  _LOG_TRACE (msg);
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
      m_log->UpdateDeviceSeqNo(deviceName, seqno);
      if (state.has_locator())
      {
        string locStr = state.locator();
        Name locatorName((const unsigned char *)locStr.c_str(), locStr.size());
    //    cout << ", Got loc: " << locatorName << endl;
        m_log->UpdateLocator(deviceName, locatorName);

        _LOG_TRACE ("self: " << m_log->GetLocalName () << ", device: " << deviceName << " < == > " << locatorName);
      }
    }
    else
    {
      _LOG_ERROR ("Receive SYNC DELETE, but we don't support it yet");
      deregister(deviceName);
    }
    index++;
  }

  // find the actuall difference and invoke callback on the actual difference
  HashPtr oldHash = m_rootHash;
  m_rootHash = m_log->RememberStateInStateLog();
  // get diff with both new SeqNo and old SeqNo
  SyncStateMsgPtr diff = m_log->FindStateDifferences(*oldHash, *m_rootHash, true);

  if (diff->state_size() > 0)
  {
    m_stateMsgCallback (diff);
  }
}

void
SyncCore::sendSyncInterest()
{
  Name syncInterest = Name (m_syncPrefix)(m_rootHash->GetHash(), m_rootHash->GetHashBytes());

  _LOG_DEBUG ("[" << m_log->GetLocalName () << "] >>> SYNC Interest for " << m_rootHash->shortHash () << ": " << syncInterest);

  Selectors selectors;
  if (m_syncInterestInterval > 0 && m_syncInterestInterval < 30.0)
  {
    selectors.interestLifetime(m_syncInterestInterval);
  }
  m_ccnx->sendInterest(syncInterest,
                         Closure (boost::bind(&SyncCore::handleSyncData, this, _1, _2),
                                  boost::bind(&SyncCore::handleSyncInterestTimeout, this, _1, _2, _3)),
                          selectors);

  // if there is a pending syncSyncInterest task, reschedule it to be m_syncInterestInterval seconds from now
  // if no such task exists, it will be added
  m_scheduler->rescheduleTask(m_sendSyncInterestTask);
}

void
SyncCore::recover(HashPtr hash)
{
  if (!(*hash == *m_rootHash) && m_log->LookupSyncLog(*hash) <= 0)
  {
    _LOG_TRACE (m_log->GetLocalName () << ", Recover for: " << hash->shortHash ());
    // unfortunately we still don't recognize this hash
    Bytes bytes;
    readRaw(bytes, (const unsigned char *)hash->GetHash(), hash->GetHashBytes());

    // append the unknown hash
    Name recoverInterest = Name (m_syncPrefix)(RECOVER)(bytes);

    _LOG_DEBUG ("[" << m_log->GetLocalName () << "] >>> RECOVER Interests for " << hash->shortHash ());

    m_ccnx->sendInterest(recoverInterest,
                         Closure (boost::bind(&SyncCore::handleRecoverData, this, _1, _2),
                                  boost::bind(&SyncCore::handleRecoverInterestTimeout, this, _1, _2, _3)));

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

sqlite3_int64
SyncCore::seq(const Name &name)
{
  return m_log->SeqNo(name);
}
