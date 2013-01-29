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

#include "fetcher.h"
#include "fetch-manager.h"
#include "ccnx-pco.h"
#include "logging.h"

#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

INIT_LOGGER ("Fetcher");

using namespace boost;
using namespace std;
using namespace Ccnx;

Fetcher::Fetcher (Ccnx::CcnxWrapperPtr ccnx,
                  const SegmentCallback &segmentCallback,
                  const FinishCallback &finishCallback,
                  OnFetchCompleteCallback onFetchComplete, OnFetchFailedCallback onFetchFailed,
                  const Ccnx::Name &deviceName, const Ccnx::Name &name, int64_t minSeqNo, int64_t maxSeqNo,
                  boost::posix_time::time_duration timeout/* = boost::posix_time::seconds (30)*/,
                  const Ccnx::Name &forwardingHint/* = Ccnx::Name ()*/)
  : m_ccnx (ccnx)

  , m_segmentCallback (segmentCallback)
  , m_onFetchComplete (onFetchComplete)
  , m_onFetchFailed (onFetchFailed)
  , m_finishCallback (finishCallback)

  , m_active (false)
  , m_name (name)
  , m_deviceName (deviceName)
  , m_forwardingHint (forwardingHint)
  , m_maximumNoActivityPeriod (timeout)

  , m_minSendSeqNo (minSeqNo-1)
  , m_maxInOrderRecvSeqNo (minSeqNo-1)
  , m_minSeqNo (minSeqNo)
  , m_maxSeqNo (maxSeqNo)

  , m_pipeline (6) // initial "congestion window"
  , m_activePipeline (0)
  , m_retryPause (0)
  , m_nextScheduledRetry (date_time::second_clock<boost::posix_time::ptime>::universal_time ())
{
}

Fetcher::~Fetcher ()
{
}

void
Fetcher::RestartPipeline ()
{
  m_active = true;
  m_minSendSeqNo = m_maxInOrderRecvSeqNo;
  // cout << "Restart: " << m_minSendSeqNo << endl;
  m_lastPositiveActivity = date_time::second_clock<boost::posix_time::ptime>::universal_time();

  // Scheduler::scheduleOneTimeTask ();
  // m_scheduler
  // m_executor.execute (bind (&Fetcher::FillPipeline, this));
  FillPipeline ();
}

void
Fetcher::SetForwardingHint (const Ccnx::Name &forwardingHint)
{
  m_forwardingHint = forwardingHint;
}

void
Fetcher::FillPipeline ()
{
  for (; m_minSendSeqNo < m_maxSeqNo && m_activePipeline < m_pipeline; m_minSendSeqNo++)
    {
      if (m_outOfOrderRecvSeqNo.find (m_minSendSeqNo+1) != m_outOfOrderRecvSeqNo.end ())
        continue;

      if (m_inActivePipeline.find (m_minSendSeqNo+1) != m_inActivePipeline.end ())
        continue;

      m_inActivePipeline.insert (m_minSendSeqNo+1);

      _LOG_DEBUG (" >>> i " << Name (m_forwardingHint)(m_name) << ", seq = " << (m_minSendSeqNo + 1 ));

      // cout << ">>> " << m_minSendSeqNo+1 << endl;
      m_ccnx->sendInterest (Name (m_forwardingHint)(m_name)(m_minSendSeqNo+1),
                            Closure (bind(&Fetcher::OnData, this, m_minSendSeqNo+1, _1, _2),
                                     bind(&Fetcher::OnTimeout, this, m_minSendSeqNo+1, _1)),
                            Selectors().interestLifetime (1)); // Alex: this lifetime should be changed to RTO
      _LOG_DEBUG (" >>> i ok");

      m_activePipeline ++;
    }
}

void
Fetcher::OnData (uint64_t seqno, const Ccnx::Name &name, PcoPtr data)
{
  _LOG_DEBUG (" <<< d " << name.getPartialName (0, name.size () - 1) << ", seq = " << seqno);

  if (m_forwardingHint == Name ())
  {
    // invoke callback
    if (!m_segmentCallback.empty ())
      {
        m_segmentCallback (m_deviceName, m_name, seqno, data);
      }
    // we don't have to tell FetchManager about this
  }
  else
    {
      try {
        PcoPtr pco = make_shared<ParsedContentObject> (*data->contentPtr ());
        if (!m_segmentCallback.empty ())
          {
            m_segmentCallback (m_deviceName, m_name, seqno, pco);
          }
      }
      catch (MisformedContentObjectException &e)
        {
          cerr << "MisformedContentObjectException..." << endl;
          // no idea what should do...
          // let's ignore for now
        }
    }

  m_activePipeline --;
  m_lastPositiveActivity = date_time::second_clock<boost::posix_time::ptime>::universal_time();

  ////////////////////////////////////////////////////////////////////////////
  m_outOfOrderRecvSeqNo.insert (seqno);
  m_inActivePipeline.erase (seqno);
  _LOG_DEBUG ("Total segments received: " << m_outOfOrderRecvSeqNo.size ());
  set<int64_t>::iterator inOrderSeqNo = m_outOfOrderRecvSeqNo.begin ();
  for (; inOrderSeqNo != m_outOfOrderRecvSeqNo.end ();
       inOrderSeqNo++)
    {
      _LOG_TRACE ("Checking " << *inOrderSeqNo << " and " << m_maxInOrderRecvSeqNo+1);
      if (*inOrderSeqNo == m_maxInOrderRecvSeqNo+1)
        {
          m_maxInOrderRecvSeqNo = *inOrderSeqNo;
        }
      else if (*inOrderSeqNo < m_maxInOrderRecvSeqNo+1) // not possible anymore, but just in case
        {
          continue;
        }
      else
        break;
    }
  m_outOfOrderRecvSeqNo.erase (m_outOfOrderRecvSeqNo.begin (), inOrderSeqNo);
  ////////////////////////////////////////////////////////////////////////////

  _LOG_TRACE ("Max in order received: " << m_maxInOrderRecvSeqNo << ", max seqNo to request: " << m_maxSeqNo);

  if (m_maxInOrderRecvSeqNo == m_maxSeqNo)
    {
      _LOG_TRACE ("Fetch finished");
      m_active = false;
      // invoke callback
      if (!m_finishCallback.empty ())
        {
          _LOG_TRACE ("Notifying callback");
          m_finishCallback(m_deviceName, m_name);
        }

      // tell FetchManager that we have finish our job
      m_onFetchComplete (*this);
    }
  else
    {
      FillPipeline ();
      // m_executor.execute (bind (&Fetcher::FillPipeline, this));
    }
}

Closure::TimeoutCallbackReturnValue
Fetcher::OnTimeout (uint64_t seqno, const Ccnx::Name &name)
{
  _LOG_DEBUG (" <<< :( timeout " << name.getPartialName (0, name.size () - 1) << ", seq = " << seqno);

  // cout << "Fetcher::OnTimeout: " << name << endl;
  // cout << "Last: " << m_lastPositiveActivity << ", config: " << m_maximumNoActivityPeriod
  //      << ", now: " << date_time::second_clock<boost::posix_time::ptime>::universal_time()
  //      << ", oldest: " << (date_time::second_clock<boost::posix_time::ptime>::universal_time() - m_maximumNoActivityPeriod) << endl;

  if (m_lastPositiveActivity <
      (date_time::second_clock<boost::posix_time::ptime>::universal_time() - m_maximumNoActivityPeriod))
    {
      m_inActivePipeline.erase (seqno);
      m_activePipeline --;
      if (m_activePipeline == 0)
        {
          _LOG_DEBUG ("Telling that fetch failed");
          _LOG_DEBUG ("Active pipeline size should be zero: " << m_inActivePipeline.size ());

          m_active = false;
          m_onFetchFailed (*this);
          // this is not valid anymore, but we still should be able finish work
        }
      return Closure::RESULT_OK;
    }
  else
    {
      _LOG_DEBUG ("Asking to reexpress seqno: " << seqno);
      return Closure::RESULT_REEXPRESS;
    }
}
