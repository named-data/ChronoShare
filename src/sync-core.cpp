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

#include "sync-core.hpp"
#include "sync-state-helper.hpp"
#include "core/logging.hpp"

#include <ndn-cxx/util/string-helper.hpp>

#include <boost/lexical_cast.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(Sync.Core);

const int SyncCore::FRESHNESS = 2;
const std::string SyncCore::RECOVER = "RECOVER";
const double SyncCore::WAIT = 0.05;
const double SyncCore::RANDOM_PERCENT = 0.5;

SyncCore::SyncCore(Face& face, SyncLogPtr syncLog, const Name& userName, const Name& localPrefix,
                   const Name& syncPrefix, const StateMsgCallback& callback,
                   time::seconds syncInterestInterval /*= -1.0*/)
  : m_face(face)
  , m_log(syncLog)
  , m_scheduler(m_face.getIoService())
  , m_syncInterestEvent(m_scheduler)
  , m_periodicInterestEvent(m_scheduler)
  , m_localStateDelayedEvent(m_scheduler)
  , m_stateMsgCallback(callback)
  , m_syncPrefix(syncPrefix)
  , m_recoverWaitGenerator(
      new RandomIntervalGenerator(WAIT, RANDOM_PERCENT, RandomIntervalGenerator::Direction::UP))
  , m_syncInterestInterval(syncInterestInterval)
{
  m_rootDigest = m_log->RememberStateInStateLog();

  m_registeredPrefixId =
    m_face.setInterestFilter(m_syncPrefix, bind(&SyncCore::handleInterest, this, _1, _2),
                             RegisterPrefixSuccessCallback(),
                             bind(&SyncCore::onRegisterFailed, this, _1, _2));

  m_log->UpdateLocalLocator(localPrefix);

  time::seconds interval =
    (m_syncInterestInterval > time::seconds(0) && m_syncInterestInterval < time::seconds(30)) ?
      m_syncInterestInterval : time::seconds(4);

  m_periodicInterestEvent =
    m_scheduler.scheduleEvent(interval, bind(&SyncCore::sendPeriodicSyncInterest, this, interval));

  m_syncInterestEvent =
    m_scheduler.scheduleEvent(time::milliseconds(100), bind(&SyncCore::sendSyncInterest, this));
}

void
SyncCore::sendPeriodicSyncInterest(const time::seconds& interval)
{
  sendSyncInterest();
  m_periodicInterestEvent =
    m_scheduler.scheduleEvent(interval, bind(&SyncCore::sendPeriodicSyncInterest, this, interval));
}

SyncCore::~SyncCore()
{
  // need to "deregister" closures
  m_face.unsetInterestFilter(m_registeredPrefixId);
}

void
SyncCore::updateLocalState(sqlite3_int64 seqno)
{
  m_log->UpdateLocalSeqNo(seqno);
  localStateChanged();
}

void
SyncCore::localStateChanged()
{
  ConstBufferPtr oldDigest = m_rootDigest;
  m_rootDigest = m_log->RememberStateInStateLog();

  _LOG_DEBUG("[" << m_log->GetLocalName() << "] localStateChanged ");
  _LOG_TRACE("[" << m_log->GetLocalName() << "] publishes: oldDigest--" << toHex(*oldDigest)
                 << " newDigest--"
                 << toHex(*m_rootDigest));

  SyncStateMsgPtr msg = m_log->FindStateDifferences(*oldDigest, *m_rootDigest);

  // reply sync Interest with oldDigest as last component

  Name syncName(m_syncPrefix);
  syncName.appendImplicitSha256Digest(oldDigest);

  BufferPtr syncData = serializeGZipMsg(*msg);

  // Create Data packet
  shared_ptr<Data> data = make_shared<Data>();
  data->setName(syncName);
  data->setFreshnessPeriod(time::seconds(FRESHNESS));
  data->setContent(reinterpret_cast<const uint8_t*>(syncData->buf()), syncData->size());
  m_keyChain.sign(*data);
  m_face.put(*data);

  _LOG_TRACE(msg);

  // no hurry in sending out new Sync Interest; if others send the new Sync Interest first, no
  // problem, we know the new root digest already;
  // this is trying to avoid the situation that the order of SyncData and new Sync Interest gets
  // reversed at receivers
  m_syncInterestEvent =
    m_scheduler.scheduleEvent(time::milliseconds(50), bind(&SyncCore::sendSyncInterest, this));


  // sendSyncInterest();
}

void
SyncCore::localStateChangedDelayed()
{
  // many calls to localStateChangedDelayed within 0.5 second will be suppressed to one
  // localStateChanged calls
  m_localStateDelayedEvent =
    m_scheduler.scheduleEvent(time::milliseconds(500), bind(&SyncCore::localStateChanged, this));
}

// ------------------------------------------------------------------------------------ send &
// handle interest

void
SyncCore::sendSyncInterest()
{
  Name syncInterest(m_syncPrefix);
  //  syncInterest.append(name::Component(*m_rootDigest));
  syncInterest.appendImplicitSha256Digest(m_rootDigest);

  _LOG_DEBUG("[" << m_log->GetLocalName() << "] >>> send SYNC Interest for " << toHex(*m_rootDigest)
                 << ": "
                 << syncInterest);

  Interest interest(syncInterest);
  if (m_syncInterestInterval > time::seconds(0) && m_syncInterestInterval < time::seconds(30)) {
    interest.setInterestLifetime(m_syncInterestInterval);
  }

  m_face.expressInterest(interest, bind(&SyncCore::handleSyncData, this, _1, _2),
                         bind(&SyncCore::handleSyncInterestTimeout, this, _1));

  // // if there is a pending syncSyncInterest task, reschedule it to be m_syncInterestInterval seconds
  // // from now
  // // if no such task exists, it will be added
  // m_scheduler->rescheduleTask(m_sendSyncInterestTask);
}

void
SyncCore::recover(ConstBufferPtr digest)
{
  if (!(*digest == *m_rootDigest) && m_log->LookupSyncLog(*digest) <= 0) {
    _LOG_TRACE(m_log->GetLocalName() << ", Recover for received_Digest " << toHex(*digest));
    // unfortunately we still don't recognize this digest
    // append the unknown digest
    Name recoverInterest(m_syncPrefix);
    recoverInterest.append(RECOVER).appendImplicitSha256Digest(digest);

    _LOG_DEBUG("[" << m_log->GetLocalName() << "] >>> send RECOVER Interests for " << toHex(*digest));

    m_face.expressInterest(recoverInterest,
                           bind(&SyncCore::handleRecoverData, this, _1, _2),
                           bind(&SyncCore::handleRecoverInterestTimeout, this, _1));
  }
  else {
    // we already learned the digest; cheers!
  }
}

void
SyncCore::handleInterest(const InterestFilter& filter, const Interest& interest)
{
  Name name = interest.getName();
  _LOG_DEBUG("[" << m_log->GetLocalName() << "] <<<< handleInterest with Name: " << name);
  int size = name.size();
  int prefixSize = m_syncPrefix.size();
  if (size == prefixSize + 1) {
    // this is normal sync interest
    handleSyncInterest(name);
  }
  else if (size == prefixSize + 2 && name.get(m_syncPrefix.size()).toUri() == RECOVER) {
    // this is recovery interest
    handleRecoverInterest(name);
  }
}

void
SyncCore::handleSyncInterest(const Name& name)
{
  _LOG_DEBUG("[" << m_log->GetLocalName() << "] <<<<< handle SYNC Interest with Name: " << name);

  ConstBufferPtr digest = make_shared<Buffer>(name.get(-1).value(), name.get(-1).value_size());
  if (*digest == *m_rootDigest) {
    // we have the same digest; nothing needs to be done
    _LOG_TRACE("same as root digest: " << toHex(*digest));
    return;
  }
  else if (m_log->LookupSyncLog(*digest) > 0) {
    // we know something more
    _LOG_TRACE("found digest in sync log");
    SyncStateMsgPtr msg = m_log->FindStateDifferences(*digest, *m_rootDigest);

    BufferPtr syncData = serializeGZipMsg(*msg);
    shared_ptr<Data> data = make_shared<Data>();
    data->setName(name);
    data->setFreshnessPeriod(time::seconds(FRESHNESS));
    data->setContent(reinterpret_cast<const uint8_t*>(syncData->buf()), syncData->size());
    m_keyChain.sign(*data);
    m_face.put(*data);

    _LOG_TRACE(m_log->GetLocalName() << " publishes: " << toHex(*digest) << " my_rootDigest:"
                                     << toHex(*m_rootDigest));
    _LOG_TRACE(msg);
  }
  else {
    // we don't recognize the digest, send recover Interest if still don't know the digest after a
    // randomized wait period
    double wait = m_recoverWaitGenerator->nextInterval();
    _LOG_TRACE(m_log->GetLocalName() << ", my_rootDigest: " << toHex(*m_rootDigest)
                                     << ", received_Digest: "
                                     << toHex(*digest));
    _LOG_TRACE("recover task scheduled after wait: " << wait);

    // @todo Figure out how to cancel scheduled events when class is destroyed
    m_scheduler.scheduleEvent(time::milliseconds(static_cast<int>(wait * 1000)),
                              bind(&SyncCore::recover, this, digest));
  }
}

void
SyncCore::handleRecoverInterest(const Name& name)
{
  _LOG_DEBUG("[" << m_log->GetLocalName() << "] <<<<< handle RECOVER Interest with name " << name);

  ConstBufferPtr digest = make_shared<Buffer>(name.get(-1).value(), name.get(-1).value_size());
  // this is the digest unkonwn to the sender of the interest
  _LOG_DEBUG("rootDigest: " << toHex(*digest));
  if (m_log->LookupSyncLog(*digest) > 0) {
    _LOG_DEBUG("Find in our sync_log! " << toHex(*digest));
    // we know the digest, should reply everything and the newest thing, but not the digest!!! This
    // is important
    unsigned char _origin = 0;
    BufferPtr origin = make_shared<Buffer>(_origin);
    //    std::cout << "size of origin " << origin->size() << std::endl;
    SyncStateMsgPtr msg = m_log->FindStateDifferences(*origin, *m_rootDigest);

    BufferPtr syncData = serializeGZipMsg(*msg);
    shared_ptr<Data> data = make_shared<Data>();
    data->setName(name);
    data->setFreshnessPeriod(time::seconds(FRESHNESS));
    data->setContent(reinterpret_cast<const uint8_t*>(syncData->buf()), syncData->size());
    m_keyChain.sign(*data);
    m_face.put(*data);

    _LOG_TRACE("[" << m_log->GetLocalName() << "] publishes " << toHex(*digest)
                   << " FindStateDifferences(0, m_rootDigest/"
                   << toHex(*m_rootDigest)
                   << ")");
    _LOG_TRACE(msg);
  }
  else {
    // we don't recognize this digest, can not help
    _LOG_DEBUG("we don't recognize this digest, can not help");
  }
}

void
SyncCore::handleSyncInterestTimeout(const Interest& interest)
{
  // sync interest will be resent by scheduler
}

void
SyncCore::handleRecoverInterestTimeout(const Interest& interest)
{
  // We do not re-express recovery interest for now
  // if difference is not resolved, the sync interest will trigger
  // recovery anyway; so it's not so important to have recovery interest
  // re-expressed
}

void
SyncCore::handleSyncData(const Interest& interest, Data& data)
{
  _LOG_DEBUG("[" << m_log->GetLocalName() << "] <<<<< receive SYNC DATA with interest: "
                 << interest.toUri());

  const Block& content = data.getContent();
  // suppress recover in interest - data out of order case
  if (data.getContent().value() && content.size() > 0) {
    handleStateData(Buffer(content.value(), content.value_size()));
  }
  else {
    _LOG_ERROR("Got sync DATA with empty content");
  }

  // resume outstanding sync interest
  m_syncInterestEvent =
    m_scheduler.scheduleEvent(time::milliseconds(0), bind(&SyncCore::sendSyncInterest, this));
}

void
SyncCore::handleRecoverData(const Interest& interest, Data& data)
{
  _LOG_DEBUG("[" << m_log->GetLocalName() << "] <<<<< receive RECOVER DATA with interest: "
                 << interest.toUri());
  // cout << "handle recover data" << end;
  const Block& content = data.getContent();
  if (content.value() && content.size() > 0) {
    handleStateData(Buffer(content.value(), content.value_size()));
  }
  else {
    _LOG_ERROR("Got recovery DATA with empty content");
  }

  m_syncInterestEvent =
    m_scheduler.scheduleEvent(time::milliseconds(0), bind(&SyncCore::sendSyncInterest, this));
}

void
SyncCore::handleStateData(const Buffer& content)
{
  SyncStateMsgPtr msg = deserializeGZipMsg<SyncStateMsg>(content);
  if (!(msg)) {
    // ignore misformed SyncData
    _LOG_ERROR("Misformed SyncData");
    return;
  }

  _LOG_TRACE("[" << m_log->GetLocalName() << "]"
                 << " receives Msg ");
  _LOG_TRACE(msg);
  int size = msg->state_size();
  int index = 0;
  while (index < size) {
    SyncState state = msg->state(index);
    std::string devStr = state.name();
    Name deviceName(Block((const unsigned char*)devStr.c_str(), devStr.size()));
    if (state.type() == SyncState::UPDATE) {
      sqlite3_int64 seqno = state.seq();
      m_log->UpdateDeviceSeqNo(deviceName, seqno);
      if (state.has_locator()) {
        std::string locStr = state.locator();
        Name locatorName(Block((const unsigned char*)locStr.c_str(), locStr.size()));
        m_log->UpdateLocator(deviceName, locatorName);

        _LOG_TRACE("self: " << m_log->GetLocalName() << ", device: " << deviceName << " < == > "
                            << locatorName);
      }
    }
    else {
      _LOG_ERROR("Receive SYNC DELETE, but we don't support it yet");
      deregister(deviceName);
    }
    index++;
  }

  // find the actuall difference and invoke callback on the actual difference
  ConstBufferPtr oldDigest = m_rootDigest;
  m_rootDigest = m_log->RememberStateInStateLog();
  // get diff with both new SeqNo and old SeqNo
  SyncStateMsgPtr diff = m_log->FindStateDifferences(*oldDigest, *m_rootDigest, true);

  if (diff->state_size() > 0) {
    m_stateMsgCallback(diff);
  }
}

void
SyncCore::deregister(const Name& name)
{
  // Do nothing for now
  // TODO: handle deregistering
}

sqlite3_int64
SyncCore::seq(const Name& name)
{
  return m_log->SeqNo(name);
}

} // namespace chronoshare
} // namespace ndn
