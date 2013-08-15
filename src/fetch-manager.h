/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef FETCH_MANAGER_H
#define FETCH_MANAGER_H

#include <boost/exception/all.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <string>
#include <list>
#include <stdint.h>
#include "scheduler.h"
#include "executor.h"
#include "ndnx-wrapper.h"
#include "fetch-task-db.h"

#include "fetcher.h"

class FetchManager
{
public:
  enum
    {
      PRIORITY_NORMAL,
      PRIORITY_HIGH
    };

  typedef boost::function<Ndnx::Name(const Ndnx::Name &)> Mapping;
  typedef boost::function<void(Ndnx::Name &deviceName, Ndnx::Name &baseName, uint64_t seq, Ndnx::PcoPtr pco)> SegmentCallback;
  typedef boost::function<void(Ndnx::Name &deviceName, Ndnx::Name &baseName)> FinishCallback;
  FetchManager (Ndnx::NdnxWrapperPtr ndnx,
                const Mapping &mapping,
                const Ndnx::Name &broadcastForwardingHint,
                uint32_t parallelFetches = 3,
                const SegmentCallback &defaultSegmentCallback = SegmentCallback(),
                const FinishCallback &defaultFinishCallback = FinishCallback(),
                const FetchTaskDbPtr &taskDb = FetchTaskDbPtr()
                );
  virtual ~FetchManager ();

  void
  Enqueue (const Ndnx::Name &deviceName, const Ndnx::Name &baseName,
           const SegmentCallback &segmentCallback, const FinishCallback &finishCallback,
           uint64_t minSeqNo, uint64_t maxSeqNo, int priority=PRIORITY_NORMAL);

  // Enqueue using default callbacks
  void
  Enqueue (const Ndnx::Name &deviceName, const Ndnx::Name &baseName,
           uint64_t minSeqNo, uint64_t maxSeqNo, int priority=PRIORITY_NORMAL);

  // only for Fetcher
  inline Ndnx::NdnxWrapperPtr
  GetNdnx ();

private:
  // Fetch Events
  void
  DidDataSegmentFetched (Fetcher &fetcher, uint64_t seqno, const Ndnx::Name &basename,
                         const Ndnx::Name &name, Ndnx::PcoPtr data);

  void
  DidNoDataTimeout (Fetcher &fetcher);

  void
  DidFetchComplete (Fetcher &fetcher, const Ndnx::Name &deviceName, const Ndnx::Name &baseName);

  void
  ScheduleFetches ();

  void
  TimedWait (Fetcher &fetcher);

private:
  Ndnx::NdnxWrapperPtr m_ndnx;
  Mapping m_mapping;

  uint32_t m_maxParallelFetches;
  uint32_t m_currentParallelFetches;
  boost::mutex m_parellelFetchMutex;

  // optimized list structure for fetch queue
  typedef boost::intrusive::member_hook< Fetcher,
                                         boost::intrusive::list_member_hook<>, &Fetcher::m_managerListHook> MemberOption;
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

Ndnx::NdnxWrapperPtr
FetchManager::GetNdnx ()
{
  return m_ndnx;
}

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;
namespace Error {
struct FetchManager : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<FetchManager> FetchManagerPtr;


#endif // FETCHER_H
