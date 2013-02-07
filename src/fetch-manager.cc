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
#include <boost/lexical_cast.hpp>

#include "simple-interval-generator.h"
#include "logging.h"

INIT_LOGGER ("FetchManager");

using namespace boost;
using namespace std;
using namespace Ccnx;

//The disposer object function
struct fetcher_disposer { void operator() (Fetcher *delete_this) { delete delete_this; } };

static const string SCHEDULE_FETCHES_TAG = "ScheduleFetches";

FetchManager::FetchManager (Ccnx::CcnxWrapperPtr ccnx,
                            const Mapping &mapping,
                            const Name &broadcastForwardingHint,
                            uint32_t parallelFetches, // = 3
                            const SegmentCallback &defaultSegmentCallback,
                            const FinishCallback &defaultFinishCallback,
                            const FetchTaskDbPtr &taskDb
                            )
  : m_ccnx (ccnx)
  , m_mapping (mapping)
  , m_maxParallelFetches (parallelFetches)
  , m_currentParallelFetches (0)
  , m_scheduler (new Scheduler)
  , m_executor (new Executor(1))
  , m_defaultSegmentCallback(defaultSegmentCallback)
  , m_defaultFinishCallback(defaultFinishCallback)
  , m_taskDb(taskDb)
  , m_broadcastHint (broadcastForwardingHint)
{
  m_scheduler->start ();
  m_executor->start();

  m_scheduleFetchesTask = Scheduler::schedulePeriodicTask (m_scheduler,
                                                           make_shared<SimpleIntervalGenerator> (300), // no need to check to often. if needed, will be rescheduled
                                                           bind (&FetchManager::ScheduleFetches, this), SCHEDULE_FETCHES_TAG);
  // resume un-finished fetches if there is any
  if (m_taskDb)
  {
    m_taskDb->foreachTask(bind(&FetchManager::Enqueue, this, _1, _2, _3, _4, _5));
  }
}

FetchManager::~FetchManager ()
{
  m_scheduler->shutdown ();
  m_executor->shutdown();

  m_ccnx.reset ();

  m_fetchList.clear_and_dispose (fetcher_disposer ());
}

// Enqueue using default callbacks
void
FetchManager::Enqueue (const Ccnx::Name &deviceName, const Ccnx::Name &baseName,
           uint64_t minSeqNo, uint64_t maxSeqNo, int priority)
{
  Enqueue(deviceName, baseName, m_defaultSegmentCallback, m_defaultFinishCallback, minSeqNo, maxSeqNo, priority);
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

  if (m_taskDb)
    {
      m_taskDb->addTask(deviceName, baseName, minSeqNo, maxSeqNo, priority);
    }

  unique_lock<mutex> lock (m_parellelFetchMutex);

  _LOG_TRACE ("++++ Create fetcher: " << baseName);
  Fetcher *fetcher = new Fetcher (m_ccnx,
                                  m_executor,
                                  segmentCallback,
                                  finishCallback,
                                  bind (&FetchManager::DidFetchComplete, this, _1, _2, _3),
                                  bind (&FetchManager::DidNoDataTimeout, this, _1),
                                  deviceName, baseName, minSeqNo, maxSeqNo,
                                  boost::posix_time::seconds (30),
                                  forwardingHint);

  switch (priority)
    {
    case PRIORITY_HIGH:
      _LOG_TRACE ("++++ Push front fetcher: " << fetcher->GetName ());
      m_fetchList.push_front (*fetcher);
      break;

    case PRIORITY_NORMAL:
    default:
      _LOG_TRACE ("++++ Push back fetcher: " << fetcher->GetName ());
      m_fetchList.push_back (*fetcher);
      break;
    }

  _LOG_DEBUG ("++++ Reschedule fetcher task");
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

      if (item->IsTimedWait ())
        {
          _LOG_DEBUG ("Item is in timed-wait");
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
      _LOG_TRACE ("++++ RESTART PIPELINE: " << item->GetName ());
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

  if (fetcher.GetForwardingHint () == m_broadcastHint)
    {
      // try again directly (hopefully with different forwarding hint

      /// @todo Handle potential exception
      Name forwardingHint;
      forwardingHint = m_mapping (fetcher.GetDeviceName ());
      fetcher.SetForwardingHint (forwardingHint);
    }
  else
    {
      fetcher.SetForwardingHint (m_broadcastHint);
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
FetchManager::DidFetchComplete (Fetcher &fetcher, const Name &deviceName, const Name &baseName)
{
  {
    unique_lock<mutex> lock (m_parellelFetchMutex);
    m_currentParallelFetches --;

    if (m_taskDb)
      {
        m_taskDb->deleteTask(deviceName, baseName);
      }
  }

  // like TCP timed-wait
  m_scheduler->scheduleOneTimeTask(m_scheduler, 10, boost::bind(&FetchManager::TimedWait, this, ref(fetcher)), boost::lexical_cast<string>(baseName));

  m_scheduler->rescheduleTaskAt (m_scheduleFetchesTask, 0);
}

void
FetchManager::TimedWait (Fetcher &fetcher)
{
    unique_lock<mutex> lock (m_parellelFetchMutex);
    _LOG_TRACE ("+++++ removing fetcher: " << fetcher.GetName ());
    m_fetchList.erase_and_dispose (FetchList::s_iterator_to (fetcher), fetcher_disposer ());
}
