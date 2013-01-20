/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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

#ifndef FETCH_MANAGER_H
#define FETCH_MANAGER_H

#include <boost/exception/all.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <list>
#include <stdint.h>
#include "scheduler.h"

#include "ccnx-wrapper.h"
#include "ccnx-tunnel.h"
#include "sync-log.h"

#include "fetcher.h"

class FetchManager
{
  enum
    {
      PRIORITY_NORMAL,
      PRIORITY_HIGH
    };

public:
  FetchManager (Ccnx::CcnxWrapperPtr ccnx, SyncLogPtr sync, uint32_t parallelFetches = 3);
  virtual ~FetchManager ();

  void
  Enqueue (const Ccnx::Name &deviceName,
           uint32_t minSeqNo, uint32_t maxSeqNo, int priority=PRIORITY_NORMAL);

  inline Ccnx::CcnxWrapperPtr
  GetCcnx ();

  inline SchedulerPtr
  GetScheduler ();

private:
  void
  ScheduleFetches ();  
  
  // Events called from Fetcher
  void
  NoDataTimeout (Fetcher &fetcher);

  void
  FetchComplete (Fetcher &fetcher);

private:
  
private:
  Ccnx::CcnxWrapperPtr m_ccnx;
  SyncLogPtr m_sync; // to access forwarding hints
  SchedulerPtr m_scheduler;

  uint32_t m_maxParallelFetches;
  uint32_t m_currentParallelFetches;
  boost::mutex m_parellelFetchMutex;

  // optimized list structure for fetch queue
  typedef boost::intrusive::member_hook< Fetcher,
                                         boost::intrusive::list_member_hook<>, &Fetcher::m_managerListHook> MemberOption;
  typedef boost::intrusive::list<Fetcher, MemberOption> FetchList;
  
  FetchList m_fetchList;
};

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

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str; 
namespace Error {
struct FetchManager : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<FetchManager> FetchManagerPtr;


#endif // FETCHER_H
