/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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

#include "fetch-manager.h"
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>
#include "logging.h"

INIT_LOGGER ("Fetch.Manager");

using namespace boost;
using namespace std;
using namespace Ccnx;

static const string BROADCAST_DOMAIN = "/ndn/broadcast/chronoshare";
//The disposer object function
struct fetcher_disposer { void operator() (Fetcher *delete_this) { delete delete_this; } };

FetchManager::FetchManager (CcnxWrapperPtr ccnx, const Mapping &mapping, uint32_t parallelFetches/* = 3*/)
  : m_ccnx (ccnx)
  , m_mapping (mapping)
  , m_maxParallelFetches (parallelFetches)
  , m_currentParallelFetches (0)

{
  m_scheduler = make_shared<Scheduler> ();
  m_scheduler->start ();
}

FetchManager::~FetchManager ()
{
  m_fetchList.clear_and_dispose (fetcher_disposer ());

  m_scheduler->shutdown ();
  m_scheduler.reset ();
}

void
FetchManager::Enqueue (const Ccnx::Name &deviceName, const Ccnx::Name &baseName,
         const SegmentCallback &segmentCallback, const FinishCallback &finishCallback,
         uint64_t minSeqNo, uint64_t maxSeqNo, int priority/*PRIORITY_NORMAL*/)
{
  // Assumption for the following code is minSeqNo <= maxSeqNo
  if (minSeqNo > maxSeqNo)
  {
    return;
  }

  // we may need to guarantee that LookupLocator will gives an answer and not throw exception...
  Name forwardingHint;
  forwardingHint = m_mapping (deviceName);

  Fetcher &fetcher = *(new Fetcher (m_ccnx,
                                    segmentCallback,
                                    finishCallback,
                                    bind (&FetchManager::DidFetchComplete, this, _1),
                                    bind (&FetchManager::DidNoDataTimeout, this, _1),
                                    deviceName, baseName, minSeqNo, maxSeqNo,
                                    boost::posix_time::seconds (30),
                                    forwardingHint));

  switch (priority)
    {
    case PRIORITY_HIGH:
      m_fetchList.push_front (fetcher);
      break;

    case PRIORITY_NORMAL:
    default:
      m_fetchList.push_back (fetcher);
      break;
    }

  ScheduleFetches (); // will start a fetch if m_currentParallelFetches is less than max, otherwise does nothing
}

void
FetchManager::ScheduleFetches ()
{
  unique_lock<mutex> lock (m_parellelFetchMutex);

  for (FetchList::iterator item = m_fetchList.begin ();
       m_currentParallelFetches < m_maxParallelFetches && item != m_fetchList.end ();
       item++)
    {
      if (item->IsActive ())
        continue;

      m_currentParallelFetches ++;
      item->RestartPipeline ();
    }
}

void
FetchManager::DidNoDataTimeout (Fetcher &fetcher)
{
  fetcher.SetForwardingHint (Ccnx::Name (BROADCAST_DOMAIN));
  {
    unique_lock<mutex> lock (m_parellelFetchMutex);
    m_currentParallelFetches --;
    // no need to do anything with the m_fetchList
  }

  ScheduleFetches ();
}

void
FetchManager::DidFetchComplete (Fetcher &fetcher)
{
  {
    unique_lock<mutex> lock (m_parellelFetchMutex);
    m_currentParallelFetches --;
    m_fetchList.erase_and_dispose (FetchList::s_iterator_to (fetcher), fetcher_disposer ());
  }

}
