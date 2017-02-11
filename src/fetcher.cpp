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

#include "fetcher.hpp"
#include "fetch-manager.hpp"
#include "core/logging.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(Fetcher);

Fetcher::Fetcher(Face& face,
                 bool isSegment,
                 const SegmentCallback& segmentCallback,
                 const FinishCallback& finishCallback,
                 const OnFetchCompleteCallback& onFetchComplete,
                 const OnFetchFailedCallback& onFetchFailed,
                 const Name& deviceName, const Name& name, int64_t minSeqNo, int64_t maxSeqNo,
                 time::milliseconds timeout, const Name& forwardingHint)
  : m_face(face)

  , m_segmentCallback(segmentCallback)
  , m_onFetchComplete(onFetchComplete)
  , m_onFetchFailed(onFetchFailed)
  , m_finishCallback(finishCallback)

  , m_active(false)
  , m_timedwait(false)
  , m_name(name)
  , m_deviceName(deviceName)
  , m_forwardingHint(forwardingHint)
  , m_maximumNoActivityPeriod(timeout)

  , m_minSendSeqNo(minSeqNo - 1)
  , m_maxInOrderRecvSeqNo(minSeqNo - 1)
  // , m_minSeqNo(minSeqNo)
  , m_maxSeqNo(maxSeqNo)

  , m_pipeline(6) // initial "congestion window"
  , m_activePipeline(0)

  , m_slowStart(false)
  , m_threshold(32767) // TODO make these values dynamic
  , m_roundCount(32767)

  , m_retryPause(time::seconds::zero())
  , m_nextScheduledRetry(time::steady_clock::now())

  , m_ioService(m_face.getIoService())
  , m_isSegment(isSegment)
{
}

Fetcher::~Fetcher()
{
}

void
Fetcher::RestartPipeline()
{
  m_active = true;
  m_minSendSeqNo = m_maxInOrderRecvSeqNo;
  // cout << "Restart: " << m_minSendSeqNo << endl;
  m_lastPositiveActivity = time::steady_clock::now();

  m_ioService.post(bind(&Fetcher::FillPipeline, this));
}

void
Fetcher::SetForwardingHint(const Name& forwardingHint)
{
  m_forwardingHint = forwardingHint;
}

void
Fetcher::FillPipeline()
{
  for (; m_minSendSeqNo < m_maxSeqNo && m_activePipeline < m_pipeline; m_minSendSeqNo++) {
    std::unique_lock<std::mutex> lock(m_seqNoMutex);

    if (m_outOfOrderRecvSeqNo.find(m_minSendSeqNo + 1) != m_outOfOrderRecvSeqNo.end())
      continue;

    if (m_inActivePipeline.find(m_minSendSeqNo + 1) != m_inActivePipeline.end())
      continue;

    m_inActivePipeline.insert(m_minSendSeqNo + 1);

    _LOG_DEBUG(
      " >>> i " << Name(m_forwardingHint).append(m_name) << ", seq = " << (m_minSendSeqNo + 1));

    // cout << ">>> " << m_minSendSeqNo+1 << endl;

    Name name = Name(m_forwardingHint).append(m_name);
    if (m_isSegment) {
       name.appendSegment(m_minSendSeqNo + 1);
    }
    else {
       name.appendNumber(m_minSendSeqNo + 1);
    }
    Interest interest(name);
    interest.setInterestLifetime(time::seconds(1));  // Alex: this lifetime should be changed to RTO
    _LOG_DEBUG("interest: " << interest);
    m_face.expressInterest(interest,
                           bind(&Fetcher::OnData, this, m_minSendSeqNo + 1, _1, _2),
                           bind(&Fetcher::OnTimeout, this, m_minSendSeqNo + 1, _1));

    _LOG_TRACE(" >>> i ok");

    m_activePipeline++;
  }
}
void
Fetcher::OnData(uint64_t seqno, const Interest& interest, Data& data)
{
  const Name& name = data.getName();
  _LOG_DEBUG(" <<< d " << name.getSubName(0, name.size() - 1) << ", seq = " << seqno);

  shared_ptr<Data> pco = make_shared<Data>(data.wireEncode());

  if (m_forwardingHint == Name()) {
    // TODO: check verified!!!!
    if (true) {
      if (m_segmentCallback != nullptr) {
        m_segmentCallback(m_deviceName, m_name, seqno, pco);
      }
    }
    else {
      _LOG_ERROR("Can not verify signature content. Name = " << data.getName());
      // probably needs to do more in the future
    }
    // we don't have to tell FetchManager about this
  }
  else {
    // in this case we don't care whether "data" is verified,  in fact, we expect it is unverified

    // we need to verify this pco and apply callback only when verified
    // TODO: check verified !!!
    if (true) {
      if (m_segmentCallback != nullptr) {
        m_segmentCallback(m_deviceName, m_name, seqno, pco);
      }
    }
    else {
      _LOG_ERROR("Can not verify signature content. Name = " << pco->getName());
      // probably needs to do more in the future
    }
  }

  m_activePipeline--;
  m_lastPositiveActivity = time::steady_clock::now();

  {
    std::unique_lock<std::mutex> lock(m_seqNoMutex);
    if(m_slowStart){
      m_pipeline++;
      if(m_pipeline == m_threshold)
        m_slowStart = false;
    }
    else{
      m_roundCount++;
      _LOG_DEBUG ("roundCount: " << m_roundCount);
      if(m_roundCount == m_pipeline){
        m_pipeline++;
        m_roundCount = 0;
      }
    }
  }

  _LOG_DEBUG ("slowStart: " << std::boolalpha << m_slowStart << " pipeline: " << m_pipeline << " threshold: " << m_threshold);


  ////////////////////////////////////////////////////////////////////////////
  std::unique_lock<std::mutex> lock(m_seqNoMutex);

  m_outOfOrderRecvSeqNo.insert(seqno);
  m_inActivePipeline.erase(seqno);
  _LOG_DEBUG("Total segments received: " << m_outOfOrderRecvSeqNo.size());
  std::set<int64_t>::iterator inOrderSeqNo = m_outOfOrderRecvSeqNo.begin();
  for (; inOrderSeqNo != m_outOfOrderRecvSeqNo.end(); inOrderSeqNo++) {
    _LOG_TRACE("Checking " << *inOrderSeqNo << " and " << m_maxInOrderRecvSeqNo + 1);
    if (*inOrderSeqNo == m_maxInOrderRecvSeqNo + 1) {
      m_maxInOrderRecvSeqNo = *inOrderSeqNo;
    }
    else if (*inOrderSeqNo < m_maxInOrderRecvSeqNo + 1) // not possible anymore, but just in case
    {
      continue;
    }
    else
      break;
  }
  m_outOfOrderRecvSeqNo.erase(m_outOfOrderRecvSeqNo.begin(), inOrderSeqNo);
  ////////////////////////////////////////////////////////////////////////////

  _LOG_TRACE("Max in order received: " << m_maxInOrderRecvSeqNo << ", max seqNo to request: " << m_maxSeqNo);

  if (m_maxInOrderRecvSeqNo == m_maxSeqNo) {
    _LOG_TRACE("Fetch finished: " << m_name);
    m_active = false;
    // invoke callback
    if (m_finishCallback != nullptr) {
      _LOG_TRACE("Notifying callback");
      m_finishCallback(m_deviceName, m_name);
    }

    // tell FetchManager that we have finish our job
    // m_onFetchComplete(*this);
    // using executor, so we won't be deleted if there is scheduled FillPipeline call
    if (m_onFetchComplete != nullptr) {
      m_timedwait = true;
      m_ioService.post(bind(m_onFetchComplete, std::ref(*this), m_deviceName, m_name));
    }
  }
  else {
    m_ioService.post(bind(&Fetcher::FillPipeline, this));
  }
}

void
Fetcher::OnTimeout(uint64_t seqno, const Interest& interest)
{
  const Name name = interest.getName();
  _LOG_DEBUG(" <<< :( timeout " << name.getSubName(0, name.size() - 1) << ", seq = " << seqno);

  // cout << "Fetcher::OnTimeout: " << name << endl;
  // cout << "Last: " << m_lastPositiveActivity << ", config: " << m_maximumNoActivityPeriod
  //      << ", now: " << date_time::second_clock<boost::posix_time::ptime>::universal_time()
  //      << ", oldest: " <<(date_time::second_clock<boost::posix_time::ptime>::universal_time() -
  //      m_maximumNoActivityPeriod) << endl;

  if (m_lastPositiveActivity <
      (time::steady_clock::now() - m_maximumNoActivityPeriod)) {
    bool done = false;
    {
      std::unique_lock<std::mutex> lock(m_seqNoMutex);
      m_inActivePipeline.erase(seqno);
      m_activePipeline--;

      if (m_activePipeline == 0) {
        done = true;
      }
    }

    if (done) {
      {
        std::unique_lock<std::mutex> lock(m_seqNoMutex);
        _LOG_DEBUG("Telling that fetch failed");
        _LOG_DEBUG("Active pipeline size should be zero: " << m_inActivePipeline.size());
      }

      m_active = false;
      if (m_onFetchFailed != nullptr) {
        m_onFetchFailed(std::ref(*this));
      }
      // this is not valid anymore, but we still should be able finish work
    }
  }
  else {
    _LOG_DEBUG("Asking to reexpress seqno: " << seqno);
    m_face.expressInterest(interest,
                           bind(&Fetcher::OnData, this, seqno, _1, _2), // TODO: correct?
                           bind(&Fetcher::OnTimeout, this, seqno, _1)); // TODO: correct?
  }
}

} // namespace chronoshare
} // namespace ndn
