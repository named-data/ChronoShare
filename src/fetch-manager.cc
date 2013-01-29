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

#include "simple-interval-generator.h"
#include "logging.h"

INIT_LOGGER ("FetchManager");

using namespace boost;
using namespace std;
using namespace Ccnx;

static const Name BROADCAST_DOMAIN = Name ("/ndn/broadcast/chronoshare");
//The disposer object function
struct fetcher_disposer { void operator() (Fetcher *delete_this) { delete delete_this; } };

static const string SCHEDULE_FETCHES_TAG = "ScheduleFetches";

FetchManager::FetchManager (CcnxWrapperPtr ccnx, const Mapping &mapping, uint32_t parallelFetches/* = 3*/)
  : m_ccnx (ccnx)
  , m_mapping (mapping)
  , m_maxParallelFetches (parallelFetches)
  , m_currentParallelFetches (0)
  , m_scheduler (new Scheduler)
{
  m_scheduler->start ();

  m_scheduleFetchesTask = Scheduler::schedulePeriodicTask (m_scheduler,
                                                           make_shared<SimpleIntervalGenerator> (300), // no need to check to often. if needed, will be rescheduled
                                                           bind (&FetchManager::ScheduleFetches, this), SCHEDULE_FETCHES_TAG);
}

FetchManager::~FetchManager ()
{
  m_scheduler->shutdown ();

  m_fetchList.clear_and_dispose (fetcher_disposer ());
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

  _LOG_DEBUG ("Reschedule fetcher task");
  m_scheduler->rescheduleTaskAt (m_scheduleFetchesTask, 0);
  // ScheduleFetches (); // will start a fetch if m_currentParallelFetches is less than max, otherwise does nothing
}

void
FetchManager::ScheduleFetches ()
{
  unique_lock<mutex> lock (m_parellelFetchMutex);

  boost::posix_time::ptime currentTime = date_time::second_clock<boost::posix_time::ptime>::universal_time ();
  boost::posix_time::ptime nextSheduleCheck = currentTime + posix_time::seconds (300); // no reason to have anything, but just in case

  for (FetchList::iterator item = m_fetchList.begin ();
       m_currentParallelFetches < m_maxParallelFetches && item != m_fetchList.end ();
       item++)
    {
      if (item->IsActive ())
        {
          _LOG_DEBUG ("Item is active");
          continue;
        }

      if (currentTime < item->GetNextScheduledRetry ())
        {
          if (item->GetNextScheduledRetry () < nextSheduleCheck)
            nextSheduleCheck = item->GetNextScheduledRetry ();

          _LOG_DEBUG ("Item is delayed");
          continue;
        }

      _LOG_DEBUG ("Start fetching of " << item->GetName ());

      m_currentParallelFetches ++;
      item->RestartPipeline ();
    }

  m_scheduler->rescheduleTaskAt (m_scheduleFetchesTask, (nextSheduleCheck - currentTime).seconds ());
}

void
FetchManager::DidNoDataTimeout (Fetcher &fetcher)
{
  _LOG_DEBUG ("No data timeout for " << fetcher.GetName () << " with forwarding hint: " << fetcher.GetForwardingHint ());

  {
    unique_lock<mutex> lock (m_parellelFetchMutex);
    m_currentParallelFetches --;
    // no need to do anything with the m_fetchList
  }

  if (fetcher.GetForwardingHint () == BROADCAST_DOMAIN)
    {
      // try again directly (hopefully with different forwarding hint

      /// @todo Handle potential exception
      Name forwardingHint;
      forwardingHint = m_mapping (fetcher.GetDeviceName ());
      fetcher.SetForwardingHint (forwardingHint);
    }
  else
    {
      fetcher.SetForwardingHint (BROADCAST_DOMAIN);
    }

  double delay = fetcher.GetRetryPause ();
  if (delay < 1) // first time
    {
      delay = 1;
    }
  else
    {
      delay = std::min (2*delay, 300.0); // 5 minutes max
    }

  fetcher.SetRetryPause (delay);
  fetcher.SetNextScheduledRetry (date_time::second_clock<boost::posix_time::ptime>::universal_time () + posix_time::seconds (delay));

  m_scheduler->rescheduleTaskAt (m_scheduleFetchesTask, 0);
}

void
FetchManager::DidFetchComplete (Fetcher &fetcher)
{
  {
    unique_lock<mutex> lock (m_parellelFetchMutex);
    m_currentParallelFetches --;
    m_fetchList.erase_and_dispose (FetchList::s_iterator_to (fetcher), fetcher_disposer ());
  }

  m_scheduler->rescheduleTaskAt (m_scheduleFetchesTask, 0);
}
