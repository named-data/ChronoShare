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

#ifndef FETCH_MANAGER_H
#define FETCH_MANAGER_H

#include "ccnx-wrapper.h"
#include "executor.h"
#include "fetch-task-db.h"
#include "scheduler.h"
#include <boost/exception/all.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <stdint.h>
#include <string>

#include "fetcher.h"

class FetchManager
{
public:
  enum { PRIORITY_NORMAL, PRIORITY_HIGH };

  typedef boost::function<Ccnx::Name(const Ccnx::Name&)> Mapping;
  typedef boost::function<void(Ccnx::Name& deviceName, Ccnx::Name& baseName, uint64_t seq, Ccnx::PcoPtr pco)>
    SegmentCallback;
  typedef boost::function<void(Ccnx::Name& deviceName, Ccnx::Name& baseName)> FinishCallback;
  FetchManager(Ccnx::CcnxWrapperPtr ccnx,
               const Mapping& mapping,
               const Ccnx::Name& broadcastForwardingHint,
               uint32_t parallelFetches = 3,
               const SegmentCallback& defaultSegmentCallback = SegmentCallback(),
               const FinishCallback& defaultFinishCallback = FinishCallback(),
               const FetchTaskDbPtr& taskDb = FetchTaskDbPtr());
  virtual ~FetchManager();

  void
  Enqueue(const Ccnx::Name& deviceName, const Ccnx::Name& baseName,
          const SegmentCallback& segmentCallback, const FinishCallback& finishCallback,
          uint64_t minSeqNo, uint64_t maxSeqNo, int priority = PRIORITY_NORMAL);

  // Enqueue using default callbacks
  void
  Enqueue(const Ccnx::Name& deviceName, const Ccnx::Name& baseName, uint64_t minSeqNo,
          uint64_t maxSeqNo, int priority = PRIORITY_NORMAL);

  // only for Fetcher
  inline Ccnx::CcnxWrapperPtr
  GetCcnx();

private:
  // Fetch Events
  void
  DidDataSegmentFetched(Fetcher& fetcher, uint64_t seqno, const Ccnx::Name& basename,
                        const Ccnx::Name& name, Ccnx::PcoPtr data);

  void
  DidNoDataTimeout(Fetcher& fetcher);

  void
  DidFetchComplete(Fetcher& fetcher, const Ccnx::Name& deviceName, const Ccnx::Name& baseName);

  void
  ScheduleFetches();

  void
  TimedWait(Fetcher& fetcher);

private:
  Ndnx::NdnxWrapperPtr m_ndnx;
  Mapping m_mapping;

  uint32_t m_maxParallelFetches;
  uint32_t m_currentParallelFetches;
  boost::mutex m_parellelFetchMutex;

  // optimized list structure for fetch queue
  typedef boost::intrusive::member_hook<Fetcher, boost::intrusive::list_member_hook<>,
                                        &Fetcher::m_managerListHook>
    MemberOption;
  typedef boost::intrusive::list<Fetcher, MemberOption> FetchList;

  FetchList m_fetchList;
  SchedulerPtr m_scheduler;
  ExecutorPtr m_executor;
  TaskPtr m_scheduleFetchesTask;
  SegmentCallback m_defaultSegmentCallback;
  FinishCallback m_defaultFinishCallback;
  FetchTaskDbPtr m_taskDb;

  const Ndnx::Name m_broadcastHint;
};

Ccnx::CcnxWrapperPtr
FetchManager::GetCcnx()
{
  return m_ndnx;
}

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;
namespace Error {
struct FetchManager : virtual boost::exception, virtual std::exception
{
};
}

typedef boost::shared_ptr<FetchManager> FetchManagerPtr;


#endif // FETCHER_H
