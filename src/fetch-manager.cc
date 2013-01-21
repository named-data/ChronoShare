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

using namespace boost;
using namespace std;
using namespace Ccnx;

//The disposer object function
struct fetcher_disposer { void operator() (Fetcher *delete_this) { delete delete_this; } };

FetchManager::FetchManager (CcnxWrapperPtr ccnx, SyncLogPtr sync, uint32_t parallelFetches/* = 3*/)
  : m_ccnx (ccnx)
  , m_sync (sync)
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
FetchManager::Enqueue (const Ccnx::Name &deviceName, uint32_t minSeqNo, uint32_t maxSeqNo, int priority/*=PRIORITY_NORMAL*/)
{
  // we may need to guarantee that LookupLocator will gives an answer and not throw exception...
  Name forwardingHint;
  try {
    forwardingHint = m_sync->LookupLocator (deviceName);
  }
  catch (Error::Db &exception) {
    // just ignore for now
  }

  Fetcher &fetcher = *(new Fetcher (m_ccnx,
                                    bind (&FetchManager::DidDataSegmentFetched, this, _1, _2, _3, _4, _5),
                                    bind (&FetchManager::DidFetchComplete, this, _1),
                                    bind (&FetchManager::DidNoDataTimeout, this, _1),
                                    deviceName, minSeqNo, maxSeqNo
                                    /* Alex: should or should not include hint initially?*/));

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
FetchManager::DidDataSegmentFetched (Fetcher &fetcher, uint32_t seqno, const Ccnx::Name &basename,
                                     const Ccnx::Name &name, const Bytes &data)
{
  // do something
}

void
FetchManager::DidNoDataTimeout (Fetcher &fetcher)
{
  fetcher.SetForwardingHint (Ccnx::Name ("/ndn/broadcast"));
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

  // ? do something else
}
