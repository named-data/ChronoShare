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

#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "sync-log.hpp"
#include "core/chronoshare-common.hpp"
#include "core/random-interval-generator.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

namespace ndn {
namespace chronoshare {

// No use this now
template <class Msg>
BufferPtr
serializeMsg(const Msg& msg)
{
  int size = msg.ByteSize();
  BufferPtr bytes = std::make_shared<Buffer>(size);
  msg.SerializeToArray(bytes->buf(), size);
  return bytes;
}

template <class Msg>
shared_ptr<Msg>
deserializeMsg(const Buffer& bytes)
{
  shared_ptr<Msg> retval(new Msg());
  if (!retval->ParseFromArray(bytes.buf(), bytes.size())) {
    // to indicate an error
    return shared_ptr<Msg>();
  }
  return retval;
}

template <class Msg>
BufferPtr
serializeGZipMsg(const Msg& msg)
{
  std::vector<char> bytes; // Bytes couldn't work
  {
    boost::iostreams::filtering_ostream out;
    out.push(boost::iostreams::gzip_compressor());    // gzip filter
    out.push(boost::iostreams::back_inserter(bytes)); // back_inserter sink

    msg.SerializeToOstream(&out);
  }
  BufferPtr uBytes = std::make_shared<Buffer>(bytes.size());
  memcpy(&(*uBytes)[0], &bytes[0], bytes.size());
  return uBytes;
}

template <class Msg>
shared_ptr<Msg>
deserializeGZipMsg(const Buffer& bytes)
{
  std::vector<char> sBytes(bytes.size());
  memcpy(&sBytes[0], &bytes[0], bytes.size());
  boost::iostreams::filtering_istream in;
  in.push(boost::iostreams::gzip_decompressor()); // gzip filter
  in.push(boost::make_iterator_range(sBytes));    // source

  shared_ptr<Msg> retval = make_shared<Msg>();
  if (!retval->ParseFromIstream(&in)) {
    // to indicate an error
    return shared_ptr<Msg>();
  }

  return retval;
}

class SyncCore
{
public:
  typedef function<void(SyncStateMsgPtr stateMsg)> StateMsgCallback;

  static const int FRESHNESS; // seconds
  static const std::string RECOVER;
  static const double WAIT;           // seconds;
  static const double RANDOM_PERCENT; // seconds;

  class Error : public boost::exception, public std::runtime_error
  {
  public:
    explicit Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

public:
  SyncCore(Face& face, SyncLogPtr syncLog, const Name& userName,
           const Name& localPrefix // routable name used by the local user
           ,
           const Name& syncPrefix // the prefix for the sync collection
           ,
           const StateMsgCallback& callback // callback when state change is detected
           ,
           time::seconds syncInterestInterval = time::seconds(0));
  ~SyncCore();

  void updateLocalState(sqlite3_int64);

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

  // ------------------ only used in test -------------------------

public:
  ConstBufferPtr
  root() const
  {
    return m_rootDigest;
  }

  sqlite3_int64
  seq(const Name& name);

private:
  void
  onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \"" << prefix << "\" in local hub's daemon ("
              << reason << ")" << std::endl;
    throw Error("ERROR: Failed to register prefix (" + reason + ")");
  }

  void
  sendSyncInterest();

  void
  sendPeriodicSyncInterest(const time::seconds& interval);

  void
  recover(ConstBufferPtr digest);

  void
  handleInterest(const InterestFilter& filter, const Interest& interest);

  void
  handleSyncInterest(const Name& name);

  void
  handleRecoverInterest(const Name& name);

  void
  handleSyncInterestTimeout(const Interest& interest);

  void
  handleRecoverInterestTimeout(const Interest& interest);

  void
  handleSyncData(const Interest& interest, Data& data);

  void
  handleRecoverData(const Interest& interest, Data& data);

  void
  handleStateData(const Buffer& content);

  void
  deregister(const Name& name);

private:
  Face& m_face;

  SyncLogPtr m_log;

  Scheduler m_scheduler;
  util::scheduler::ScopedEventId m_syncInterestEvent;
  util::scheduler::ScopedEventId m_periodicInterestEvent;
  util::scheduler::ScopedEventId m_localStateDelayedEvent;

  StateMsgCallback m_stateMsgCallback;

  Name m_syncPrefix;
  ConstBufferPtr m_rootDigest;

  IntervalGeneratorPtr m_recoverWaitGenerator;

  time::seconds m_syncInterestInterval;
  KeyChain m_keyChain;
  const RegisteredPrefixId* m_registeredPrefixId;
};

} // namespace chronoshare
} // namespace ndn

#endif // SYNC_CORE_H
