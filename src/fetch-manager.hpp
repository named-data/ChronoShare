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

#ifndef FETCH_MANAGER_H
#define FETCH_MANAGER_H

#include "fetch-task-db.hpp"
#include "fetcher.hpp"
#include "core/chronoshare-common.hpp"

#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <list>

namespace ndn {
namespace chronoshare {

class FetchManager
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };

  enum { PRIORITY_NORMAL, PRIORITY_HIGH };

  typedef function<Name(const Name&)> Mapping;
  typedef function<void(Name& deviceName, Name& baseName, uint64_t seq, shared_ptr<Data> data)> SegmentCallback;
  typedef function<void(Name& deviceName, Name& baseName)> FinishCallback;

public:
  FetchManager(Face& face, const Mapping& mapping, const Name& broadcastForwardingHint,
               uint32_t parallelFetches = 3,
               const SegmentCallback& defaultSegmentCallback = SegmentCallback(),
               const FinishCallback& defaultFinishCallback = FinishCallback(),
               const FetchTaskDbPtr& taskDb = FetchTaskDbPtr());
  virtual ~FetchManager();

  void
  Enqueue(const Name& deviceName, const Name& baseName, const SegmentCallback& segmentCallback,
          const FinishCallback& finishCallback, uint64_t minSeqNo, uint64_t maxSeqNo,
          int priority = PRIORITY_NORMAL);

  // Enqueue using default callbacks
  void
  Enqueue(const Name& deviceName, const Name& baseName, uint64_t minSeqNo, uint64_t maxSeqNo,
          int priority = PRIORITY_NORMAL);

private:
  // Fetch Events
  void
  DidDataSegmentFetched(Fetcher& fetcher, uint64_t seqno, const Name& basename, const Name& name,
                        shared_ptr<Data> data);

  void
  DidNoDataTimeout(Fetcher& fetcher);

  void
  DidFetchComplete(Fetcher& fetcher, const Name& deviceName, const Name& baseName);

  void
  ScheduleFetches();

  void
  TimedWait(Fetcher& fetcher);

private:
  Face& m_face;
  Mapping m_mapping;

  uint32_t m_maxParallelFetches;
  uint32_t m_currentParallelFetches;
  std::mutex m_parellelFetchMutex;

  // optimized list structure for fetch queue
  typedef boost::intrusive::member_hook<Fetcher, boost::intrusive::list_member_hook<>,
                                        &Fetcher::m_managerListHook> MemberOption;
  typedef boost::intrusive::list<Fetcher, MemberOption> FetchList;

  FetchList m_fetchList;
  Scheduler m_scheduler;
  util::scheduler::ScopedEventId m_scheduledFetchesEvent;

  SegmentCallback m_defaultSegmentCallback;
  FinishCallback m_defaultFinishCallback;
  FetchTaskDbPtr m_taskDb;

  const Name m_broadcastHint;
  boost::asio::io_service& m_ioService;
};

typedef shared_ptr<FetchManager> FetchManagerPtr;

} // namespace chronoshare
} // namespace ndn

#endif // FETCHER_H
