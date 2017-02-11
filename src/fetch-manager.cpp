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

#include "fetch-manager.hpp"
#include "core/logging.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(FetchManager);

// The disposer object function
struct fetcher_disposer
{
  void
  operator()(Fetcher* delete_this)
  {
    delete delete_this;
  }
};

FetchManager::FetchManager(Face& face, const Mapping& mapping, const Name& broadcastForwardingHint,
                           uint32_t parallelFetches, // = 3
                           bool isSegment,
                           const SegmentCallback& defaultSegmentCallback,
                           const FinishCallback& defaultFinishCallback, const FetchTaskDbPtr& taskDb)
  : m_face(face)
  , m_mapping(mapping)
  , m_maxParallelFetches(parallelFetches)
  , m_currentParallelFetches(0)
  , m_scheduler(m_face.getIoService())
  , m_scheduledFetchesEvent(m_scheduler)
  , m_defaultSegmentCallback(defaultSegmentCallback)
  , m_defaultFinishCallback(defaultFinishCallback)
  , m_taskDb(taskDb)
  , m_broadcastHint(broadcastForwardingHint)
  , m_ioService(m_face.getIoService())
  , m_isSegment(isSegment)
{
  // no need to check to often. if needed, will be rescheduled
  m_scheduledFetchesEvent =
    m_scheduler.scheduleEvent(time::seconds(300), bind(&FetchManager::ScheduleFetches, this));

  // resume un-finished fetches if there is any
  if (m_taskDb) {
    m_taskDb->foreachTask(
      [this](const Name& deviceName, const Name& baseName, uint64_t minSeqNo, uint64_t maxSeqNo,
             int priority) { this->Enqueue(deviceName, baseName, minSeqNo, maxSeqNo, priority); });
  }
}

FetchManager::~FetchManager()
{
  m_fetchList.clear_and_dispose(fetcher_disposer());
}

// Enqueue using default callbacks
void
FetchManager::Enqueue(const Name& deviceName, const Name& baseName, uint64_t minSeqNo,
                      uint64_t maxSeqNo, int priority)
{
  Enqueue(deviceName, baseName, m_defaultSegmentCallback, m_defaultFinishCallback, minSeqNo,
          maxSeqNo, priority);
}

void
FetchManager::Enqueue(const Name& deviceName, const Name& baseName,
                      const SegmentCallback& segmentCallback, const FinishCallback& finishCallback,
                      uint64_t minSeqNo, uint64_t maxSeqNo, int priority /*PRIORITY_NORMAL*/)
{
  // Assumption for the following code is minSeqNo <= maxSeqNo
  if (minSeqNo > maxSeqNo) {
    return;
  }

  // we may need to guarantee that LookupLocator will gives an answer and not throw exception...
  Name forwardingHint;
  forwardingHint = m_mapping(deviceName);

  if (m_taskDb) {
    m_taskDb->addTask(deviceName, baseName, minSeqNo, maxSeqNo, priority);
  }

  std::unique_lock<std::mutex> lock(m_parellelFetchMutex);

  _LOG_TRACE("++++ Create fetcher: " << baseName);
  Fetcher* fetcher =
    new Fetcher(m_face, m_isSegment, segmentCallback, finishCallback,
                bind(&FetchManager::DidFetchComplete, this, _1, _2, _3),
                bind(&FetchManager::DidNoDataTimeout, this, _1), deviceName, baseName, minSeqNo,
                maxSeqNo, time::seconds(30), forwardingHint);

  switch (priority) {
    case PRIORITY_HIGH:
      _LOG_TRACE("++++ Push front fetcher: " << fetcher->GetName());
      m_fetchList.push_front(*fetcher);
      break;

    case PRIORITY_NORMAL:
    default:
      _LOG_TRACE("++++ Push back fetcher: " << fetcher->GetName());
      m_fetchList.push_back(*fetcher);
      break;
  }

  _LOG_DEBUG("++++ Reschedule fetcher task");
  m_scheduledFetchesEvent =
    m_scheduler.scheduleEvent(time::seconds(0), bind(&FetchManager::ScheduleFetches, this));
}

void
FetchManager::ScheduleFetches()
{
  std::unique_lock<std::mutex> lock(m_parellelFetchMutex);

  auto currentTime = time::steady_clock::now();
  auto nextSheduleCheck = currentTime + time::seconds(300); // no reason to have anything, but just in case

  for (FetchList::iterator item = m_fetchList.begin();
       m_currentParallelFetches < m_maxParallelFetches && item != m_fetchList.end();
       item++) {
    if (item->IsActive()) {
      _LOG_DEBUG("Item is active");
      continue;
    }

    if (item->IsTimedWait()) {
      _LOG_DEBUG("Item is in timed-wait");
      continue;
    }

    if (currentTime < item->GetNextScheduledRetry()) {
      if (item->GetNextScheduledRetry() < nextSheduleCheck) {
        nextSheduleCheck = item->GetNextScheduledRetry();
      }

      _LOG_DEBUG("Item is delayed");
      continue;
    }

    _LOG_DEBUG("Start fetching of " << item->GetName());

    m_currentParallelFetches++;
    _LOG_TRACE("++++ RESTART PIPELINE: " << item->GetName());
    item->RestartPipeline();
  }

  m_scheduledFetchesEvent = m_scheduler.scheduleEvent(nextSheduleCheck - currentTime,
                                                      bind(&FetchManager::ScheduleFetches, this));
}

void
FetchManager::DidNoDataTimeout(Fetcher& fetcher)
{
  _LOG_DEBUG("No data timeout for " << fetcher.GetName() << " with forwarding hint: "
                                    << fetcher.GetForwardingHint());

  {
    std::unique_lock<std::mutex> lock(m_parellelFetchMutex);
    m_currentParallelFetches--;
    // no need to do anything with the m_fetchList
  }

  if (fetcher.GetForwardingHint().size() == 0) {
    // will be tried initially and again after empty forwarding hint

    /// @todo Handle potential exception
    Name forwardingHint;
    forwardingHint = m_mapping(fetcher.GetDeviceName());

    if (forwardingHint.size() == 0) {
      // make sure that we will try broadcast forwarding hint eventually
      fetcher.SetForwardingHint(m_broadcastHint);
    }
    else {
      fetcher.SetForwardingHint(forwardingHint);
    }
  }
  else if (fetcher.GetForwardingHint() == m_broadcastHint) {
    // will be tried after broadcast forwarding hint
    fetcher.SetForwardingHint(Name("/"));
  }
  else {
    // will be tried after normal forwarding hint
    fetcher.SetForwardingHint(m_broadcastHint);
  }

  time::seconds delay = fetcher.GetRetryPause();
  if (delay < time::seconds(1)) // first time
  {
    delay = time::seconds(1);
  }
  else {
    delay = std::min(2 * delay, time::seconds(300)); // 5 minutes max
  }

  fetcher.SetRetryPause(delay);
  fetcher.SetNextScheduledRetry(time::steady_clock::now() + time::seconds(delay));

  m_scheduledFetchesEvent = m_scheduler.scheduleEvent(time::seconds(0), bind(&FetchManager::ScheduleFetches, this));
}

void
FetchManager::DidFetchComplete(Fetcher& fetcher, const Name& deviceName, const Name& baseName)
{
  {
    std::unique_lock<std::mutex> lock(m_parellelFetchMutex);
    m_currentParallelFetches--;

    if (m_taskDb) {
      m_taskDb->deleteTask(deviceName, baseName);
    }
  }

  // like TCP timed-wait
  m_scheduler.scheduleEvent(time::seconds(10), bind(&FetchManager::TimedWait, this, ref(fetcher)));
  m_scheduledFetchesEvent = m_scheduler.scheduleEvent(time::seconds(0), bind(&FetchManager::ScheduleFetches, this));
}

void
FetchManager::TimedWait(Fetcher& fetcher)
{
  std::unique_lock<std::mutex> lock(m_parellelFetchMutex);
  _LOG_TRACE("+++++ removing fetcher: " << fetcher.GetName());
  m_fetchList.erase_and_dispose(FetchList::s_iterator_to(fetcher), fetcher_disposer());
}

} // namespace chronoshare
} // namespace ndn
