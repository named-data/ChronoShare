/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef SYNC_CORE_H
#define SYNC_CORE_H

#include "sync-log.h"
#include <ccnx-wrapper.h>
#include <event-scheduler.h>

using namespace std;
using namespace Ccnx;

class SyncCore
{
public:
  SyncCore(const string &path, const Name &localName, CcnxWrapperPtr handle, SchedulerPtr scheduler);
  ~SyncCore();

protected:
  SyncLog m_log;
  CcnxWrapperPtr m_handle;
  SchedulerPtr m_scheduler;
};

#endif // SYNC_CORE_H
