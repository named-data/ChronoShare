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

#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

using namespace boost;
using namespace std;
using namespace Ccnx;

Fetcher::Fetcher (FetchManager &fetchManger,
                  const Ccnx::Name &name, int32_t minSeqNo, int32_t maxSeqNo,
                  const Ccnx::Name &forwardingHint/* = Ccnx::Name ()*/)
  : m_fetchManager (fetchManger)
  , m_active (false)
  , m_name (name)
  , m_forwardingHint (forwardingHint)
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

  FillPipeline ();
}

void
Fetcher::FillPipeline ()
{
  for (; m_minSendSeqNo < m_maxSeqNo && m_activePipeline < m_pipeline; m_minSendSeqNo++)
    {
      m_fetchManager.GetCcnx ()
        ->sendInterest (Name (m_name)("file")(m_minSendSeqNo+1),
                        Closure (bind(&Fetcher::OnData, this, m_minSendSeqNo+1, _1, _2),
                                 bind(&Fetcher::OnTimeout, this, m_minSendSeqNo+1, _1)));

      m_activePipeline ++;
    }
}

void
Fetcher::OnData (uint32_t seqno, const Ccnx::Name &name, const Ccnx::Bytes &)
{
  m_activePipeline --;

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

  FillPipeline ();
  // bla bla
  if (0)
    {
      m_active = false;
      m_fetchManager.DidFetchComplete (*this);
    }
}

Closure::TimeoutCallbackReturnValue
Fetcher::OnTimeout (uint32_t seqno, const Ccnx::Name &name)
{
  // Closure::RESULT_REEXPRESS
  return Closure::RESULT_OK;
}
