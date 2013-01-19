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
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

using namespace boost;
using namespace std;

Fetcher::Fetcher (Ccnx::CcnxWrapperPtr ccnx, SchedulerPtr scheduler,
                  const Ccnx::Name &name, int32_t minSeqNo, int32_t maxSeqNo,
                  const Ccnx::Name &forwardingHint/* = Ccnx::Name ()*/)
  : m_ccnx (ccnx)
  , m_scheduler (scheduler)
  , m_name (name)
  , m_forwardingHint (forwardingHint)
  , m_minSendSeqNo (-1)
  , m_maxSendSeqNo (-1)
  , m_minSeqNo (minSeqNo)
  , m_maxSeqNo (maxSeqNo)

  , m_pipeline (6) // initial "congestion window"
{
}

Fetcher::~Fetcher ()
{
}
