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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "one-time-task.h"
#include "scheduler.h"

OneTimeTask::OneTimeTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, double delay)
            : Task(callback, tag, scheduler)
{
  setTv(delay);
}

void
OneTimeTask::run()
{
  if (!m_invoked)
  {
    m_callback();
    m_invoked = true;
    deregisterSelf();
  }
}

void
OneTimeTask::deregisterSelf()
{
  m_scheduler->deleteTask(m_tag);
}

void
OneTimeTask::reset()
{
  m_invoked = false;
}
