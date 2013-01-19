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

FetchManager::FetchManager (CcnxWrapperPtr ccnx, SyncLogPtr sync)
  : m_ccnx (ccnx)
  , m_sync (sync)
{
  m_scheduler = make_shared<Scheduler> ();
  m_scheduler->start ();
}

FetchManager::~FetchManager ()
{
  m_scheduler->shutdown ();
  m_scheduler.reset ();
}

void
FetchManager::Enqueue (const Ccnx::Name &deviceName, uint32_t minSeqNo, uint32_t maxSeqNo, int priority/*=PRIORITY_NORMAL*/)
{
}

Ccnx::CcnxWrapperPtr
FetchManager::GetCcnx ()
{
  return m_ccnx;
}

SchedulerPtr
FetchManager::GetScheduler ()
{
  return m_scheduler;
}
