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

#include "task.h"
#include "scheduler.h"

static void
eventCallback(evutil_socket_t fd, short what, void *arg)
{
  Task *task = static_cast<Task *>(arg);
  task->run();
  task = NULL;
}

Task::Task(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler)
     : m_callback(callback)
     , m_tag(tag)
     , m_scheduler(scheduler)
     , m_invoked(false)
     , m_event(NULL)
     , m_tv(NULL)
{
  m_event = evtimer_new(scheduler->base(), eventCallback, this);
  m_tv = new timeval;
}

Task::~Task()
{
  if (m_event != NULL)
  {
    event_free(m_event);
    m_event = NULL;
  }

  if (m_tv != NULL)
  {
    delete m_tv;
    m_tv = NULL;
  }
}

void
Task::setTv(double delay)
{
  // Alex: when using abs function, i would recommend use it with std:: prefix, otherwise
  // the standard one may be used, which converts everything to INT, making a lot of problems
  double intPart, fraction;
  fraction = modf(std::abs(delay), &intPart);

  m_tv->tv_sec = static_cast<int>(intPart);
  m_tv->tv_usec = static_cast<int>((fraction * 1000000));
}
