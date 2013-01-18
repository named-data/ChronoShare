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

#include "periodic-task.h"
#include <utility>

PeriodicTask::PeriodicTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, const IntervalGeneratorPtr &generator)
             : Task(callback, tag, scheduler)
             , m_generator(generator)
{
}

void
PeriodicTask::run()
{
  if (!m_invoked)
  {
    m_callback();
    m_invoked = true;
    m_scheduler->rescheduleTask(m_tag);
  }
}

void
PeriodicTask::reset()
{
  m_invoked = false;
  double interval = m_generator->nextInterval();
  setTv(interval);
}
