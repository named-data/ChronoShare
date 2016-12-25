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

#include "fetcher.h"
#include "ccnx-pco.h"
#include "fetch-manager.h"
#include "logging.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

INIT_LOGGER("Fetcher");

using namespace boost;
using namespace std;
using namespace Ndnx;

Fetcher::Fetcher(Ccnx::CcnxWrapperPtr ccnx, ExecutorPtr executor,
                 const SegmentCallback& segmentCallback, const FinishCallback& finishCallback,
                 OnFetchCompleteCallback onFetchComplete, OnFetchFailedCallback onFetchFailed,
                 const Ccnx::Name& deviceName, const Ccnx::Name& name, int64_t minSeqNo,
                 int64_t maxSeqNo,
                 boost::posix_time::time_duration timeout /* = boost::posix_time::seconds (30)*/,
                 const Ccnx::Name& forwardingHint /* = Ccnx::Name ()*/)
  : m_ccnx(ccnx)

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
  , m_minSeqNo(minSeqNo)
  , m_maxSeqNo(maxSeqNo)

  , m_pipeline(6) // initial "congestion window"
  , m_activePipeline(0)
  , m_retryPause(0)
  , m_nextScheduledRetry(date_time::second_clock<boost::posix_time::ptime>::universal_time())
  , m_executor(executor) // must be 1
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
  m_lastPositiveActivity = date_time::second_clock<boost::posix_time::ptime>::universal_time();

  m_executor->execute(bind(&Fetcher::FillPipeline, this));
}

void
Fetcher::SetForwardingHint(const Ccnx::Name& forwardingHint)
{
  m_forwardingHint = forwardingHint;
}

void
Fetcher::FillPipeline()
{
  for (; m_minSendSeqNo < m_maxSeqNo && m_activePipeline < m_pipeline; m_minSendSeqNo++) {
    unique_lock<mutex> lock(m_seqNoMutex);

    if (m_outOfOrderRecvSeqNo.find(m_minSendSeqNo + 1) != m_outOfOrderRecvSeqNo.end())
      continue;

    if (m_inActivePipeline.find(m_minSendSeqNo + 1) != m_inActivePipeline.end())
      continue;

    m_inActivePipeline.insert(m_minSendSeqNo + 1);

    _LOG_DEBUG(" >>> i " << Name(m_forwardingHint)(m_name) << ", seq = " << (m_minSendSeqNo + 1));

    // cout << ">>> " << m_minSendSeqNo+1 << endl;
    m_ccnx->sendInterest(Name(m_forwardingHint)(m_name)(m_minSendSeqNo + 1),
                         Closure(bind(&Fetcher::OnData, this, m_minSendSeqNo + 1, _1, _2),
                                 bind(&Fetcher::OnTimeout, this, m_minSendSeqNo + 1, _1, _2, _3)),
                         Selectors().interestLifetime(1)); // Alex: this lifetime should be changed to RTO
    _LOG_DEBUG(" >>> i ok");

    m_activePipeline++;
  }
}

void
Fetcher::OnData(uint64_t seqno, const Ccnx::Name& name, PcoPtr data)
{
  m_executor->execute(bind(&Fetcher::OnData_Execute, this, seqno, name, data));
}

void
Fetcher::OnData_Execute(uint64_t seqno, Ccnx::Name name, Ccnx::PcoPtr data)
{
  _LOG_DEBUG(" <<< d " << name.getPartialName(0, name.size() - 1) << ", seq = " << seqno);

  if (m_forwardingHint == Name()) {
    // TODO: check verified!!!!
    if (true) {
      if (!m_segmentCallback.empty()) {
        m_segmentCallback(m_deviceName, m_name, seqno, data);
      }
    }
    else {
      _LOG_ERROR("Can not verify signature content. Name = " << data->name());
      // probably needs to do more in the future
    }
    // we don't have to tell FetchManager about this
  }
  else {
    // in this case we don't care whether "data" is verified,  in fact, we expect it is unverified
    try {
      PcoPtr pco = make_shared<ParsedContentObject>(*data->contentPtr());

      // we need to verify this pco and apply callback only when verified
      // TODO: check verified !!!
      if (true) {
        if (!m_segmentCallback.empty()) {
          m_segmentCallback(m_deviceName, m_name, seqno, pco);
        }
      }
      else {
        _LOG_ERROR("Can not verify signature content. Name = " << pco->name());
        // probably needs to do more in the future
      }
    }
    catch (MisformedContentObjectException& e) {
      cerr << "MisformedContentObjectException..." << endl;
      // no idea what should do...
      // let's ignore for now
    }
  }

  m_activePipeline--;
  m_lastPositiveActivity = date_time::second_clock<boost::posix_time::ptime>::universal_time();

  {
    unique_lock<mutex> lock (m_pipelineMutex);
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

  _LOG_DEBUG ("slowStart: " << boolalpha << m_slowStart << " pipeline: " << m_pipeline << " threshold: " << m_threshold);


  ////////////////////////////////////////////////////////////////////////////
  unique_lock<mutex> lock(m_seqNoMutex);

  m_outOfOrderRecvSeqNo.insert(seqno);
  m_inActivePipeline.erase(seqno);
  _LOG_DEBUG("Total segments received: " << m_outOfOrderRecvSeqNo.size());
  set<int64_t>::iterator inOrderSeqNo = m_outOfOrderRecvSeqNo.begin();
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

  _LOG_TRACE("Max in order received: " << m_maxInOrderRecvSeqNo
                                       << ", max seqNo to request: " << m_maxSeqNo);

  if (m_maxInOrderRecvSeqNo == m_maxSeqNo) {
    _LOG_TRACE("Fetch finished: " << m_name);
    m_active = false;
    // invoke callback
    if (!m_finishCallback.empty()) {
      _LOG_TRACE("Notifying callback");
      m_finishCallback(m_deviceName, m_name);
    }

    // tell FetchManager that we have finish our job
    // m_onFetchComplete (*this);
    // using executor, so we won't be deleted if there is scheduled FillPipeline call
    if (!m_onFetchComplete.empty()) {
      m_timedwait = true;
      m_executor->execute(bind(m_onFetchComplete, ref(*this), m_deviceName, m_name));
    }
  }
  else {
    m_executor->execute(bind(&Fetcher::FillPipeline, this));
  }
}

void
Fetcher::OnTimeout(uint64_t seqno, const Ccnx::Name& name, const Closure& closure, Selectors selectors)
{
  _LOG_DEBUG(this << ", " << m_executor.get());
  m_executor->execute(bind(&Fetcher::OnTimeout_Execute, this, seqno, name, closure, selectors));
}

void
Fetcher::OnTimeout_Execute(uint64_t seqno, Ccnx::Name name, Ccnx::Closure closure,
                           Ccnx::Selectors selectors)
{
  _LOG_DEBUG(" <<< :( timeout " << name.getPartialName(0, name.size() - 1) << ", seq = " << seqno);

  // cout << "Fetcher::OnTimeout: " << name << endl;
  // cout << "Last: " << m_lastPositiveActivity << ", config: " << m_maximumNoActivityPeriod
  //      << ", now: " << date_time::second_clock<boost::posix_time::ptime>::universal_time()
  //      << ", oldest: " << (date_time::second_clock<boost::posix_time::ptime>::universal_time() - m_maximumNoActivityPeriod) << endl;

  if (m_lastPositiveActivity < (date_time::second_clock<boost::posix_time::ptime>::universal_time() -
                                m_maximumNoActivityPeriod)) {
    bool done = false;
    {
      unique_lock<mutex> lock(m_seqNoMutex);
      m_inActivePipeline.erase(seqno);
      m_activePipeline--;

      if (m_activePipeline == 0) {
        done = true;
      }
    }

    if (done) {
      {
        unique_lock<mutex> lock(m_seqNoMutex);
        _LOG_DEBUG("Telling that fetch failed");
        _LOG_DEBUG("Active pipeline size should be zero: " << m_inActivePipeline.size());
      }

      m_active = false;
      if (!m_onFetchFailed.empty()) {
        m_onFetchFailed(ref(*this));
      }
      // this is not valid anymore, but we still should be able finish work
    }
  }
  else {
    _LOG_DEBUG("Asking to reexpress seqno: " << seqno);
    m_ccnx->sendInterest(name, closure, selectors);
  }
}
