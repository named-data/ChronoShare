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

#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost;
using namespace std;
using namespace Ccnx;

Fetcher::Fetcher (CcnxWrapperPtr ccnx,
                  OnDataSegmentCallback onDataSegment,
                  OnFetchCompleteCallback onFetchComplete, OnFetchFailedCallback onFetchFailed,
                  const Ccnx::Name &name, int32_t minSeqNo, int32_t maxSeqNo,
                  boost::posix_time::time_duration timeout/* = boost::posix_time::seconds (30)*/,
                  const Ccnx::Name &forwardingHint/* = Ccnx::Name ()*/)
  : m_ccnx (ccnx)

  , m_onDataSegment (onDataSegment)
  , m_onFetchComplete (onFetchComplete)
  , m_onFetchFailed (onFetchFailed)

  , m_active (false)
  , m_name (name)
  , m_forwardingHint (forwardingHint)
  , m_maximumNoActivityPeriod (timeout)

  , m_minSendSeqNo (-1)
  , m_maxInOrderRecvSeqNo (-1)
  , m_minSeqNo (minSeqNo)
  , m_maxSeqNo (maxSeqNo)

  , m_pipeline (6) // initial "congestion window"
  , m_activePipeline (0)
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

      // cout << ">>> " << m_minSendSeqNo+1 << endl;
      m_ccnx->sendInterest (Name (m_forwardingHint)(m_name)(m_minSendSeqNo+1),
                            Closure (bind(&Fetcher::OnData, this, m_minSendSeqNo+1, _1, _2),
                                     bind(&Fetcher::OnTimeout, this, m_minSendSeqNo+1, _1)),
                            Selectors().interestLifetime (1)); // Alex: this lifetime should be changed to RTO

      m_activePipeline ++;
    }
}

void
Fetcher::OnData (uint32_t seqno, const Ccnx::Name &name, const Ccnx::Bytes &content)
{
  if (m_forwardingHint == Name ())
    m_onDataSegment (*this, seqno, m_name, name, content);
  else
    {
      try {
        ParsedContentObject pco (content);
        m_onDataSegment (*this, seqno, m_name, pco.name (), pco.content ());
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
  set<int32_t>::iterator inOrderSeqNo = m_outOfOrderRecvSeqNo.begin ();
  for (; inOrderSeqNo != m_outOfOrderRecvSeqNo.end ();
       inOrderSeqNo++)
    {
      if (*inOrderSeqNo == m_maxInOrderRecvSeqNo+1)
        {
          m_maxInOrderRecvSeqNo = *inOrderSeqNo;
        }
      else
        break;
    }
  m_outOfOrderRecvSeqNo.erase (m_outOfOrderRecvSeqNo.begin (), inOrderSeqNo);
  ////////////////////////////////////////////////////////////////////////////

  if (m_maxInOrderRecvSeqNo == m_maxSeqNo)
    {
      m_active = false;
      m_onFetchComplete (*this);
    }
  else
    {
      FillPipeline ();
    }
}

Closure::TimeoutCallbackReturnValue
Fetcher::OnTimeout (uint32_t seqno, const Ccnx::Name &name)
{
  // cout << "Fetcher::OnTimeout: " << name << endl;
  // cout << "Last: " << m_lastPositiveActivity << ", config: " << m_maximumNoActivityPeriod
  //      << ", now: " << date_time::second_clock<boost::posix_time::ptime>::universal_time()
  //      << ", oldest: " << (date_time::second_clock<boost::posix_time::ptime>::universal_time() - m_maximumNoActivityPeriod) << endl;

  if (m_lastPositiveActivity <
      (date_time::second_clock<boost::posix_time::ptime>::universal_time() - m_maximumNoActivityPeriod))
    {
      m_activePipeline --;
      if (m_activePipeline == 0)
        {
          m_active = false;
          m_onFetchFailed (*this);
          // this is not valid anymore, but we still should be able finish work
        }
      return Closure::RESULT_OK;
    }
  else
    return Closure::RESULT_REEXPRESS;
}
