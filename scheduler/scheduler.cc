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

#include "scheduler.h"
#include <utility>

using namespace std;

#define EVLOOP_NO_EXIT_ON_EMPTY 0x04

// IntervalGeneratorPtr
// IntervalGenerator:: Null;

void errorCallback(int err)
{
  cout << "Fatal error: " << err << endl;
}

Scheduler::Scheduler()
  : m_running(false)
{
  event_set_fatal_callback(errorCallback);
  evthread_use_pthreads();
  m_base = event_base_new();
}

Scheduler::~Scheduler()
{
  event_base_free(m_base);
}

void
Scheduler::eventLoop()
{
  while(true)
  {
    if (event_base_loop(m_base, EVLOOP_NO_EXIT_ON_EMPTY) < 0)
    {
      cout << "scheduler loop break error" << endl;
    }
    ReadLock lock(m_mutex);
    if (!m_running)
    {
      cout << "scheduler loop break normal" << endl;
      break;
    }
  }
}

void
Scheduler::start()
{
  WriteLock lock(m_mutex);
  if (!m_running)
  {
    m_thread = boost::thread(&Scheduler::eventLoop, this);
    m_running = true;
  }
}

void
Scheduler::shutdown()
{
  WriteLock lock(m_mutex);
  if (m_running)
  {
    event_base_loopbreak(m_base);
    m_thread.join();
    m_running = false;
  }
}

bool
Scheduler::addTask(const TaskPtr &task)
{
  TaskPtr newTask = task;

  if (addToMap(newTask))
  {
    newTask->reset();
    int res = evtimer_add(newTask->ev(), newTask->tv());
    if (res < 0)
    {
      cout << "evtimer_add failed for " << task->tag() << endl;
    }
    return true;
  }
  else
  {
    cout << "fail to add task: " << task->tag() << endl;
  }

  return false;
}

void
Scheduler::rescheduleTask(const Task::Tag &tag)
{
  ReadLock lock(m_mutex);
  TaskMapIt it = m_taskMap.find(tag);
  if (it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    task->reset();
    int res = evtimer_add(task->ev(), task->tv());
    if (res < 0)
    {
      cout << "evtimer_add failed for " << task->tag() << endl;
    }
  }
}

bool
Scheduler::addToMap(const TaskPtr &task)
{
  WriteLock lock(m_mutex);
  if (m_taskMap.find(task->tag()) == m_taskMap.end())
  {
    m_taskMap.insert(make_pair(task->tag(), task));
    return true;
  }
  return false;
}

void
Scheduler::deleteTask(const Task::Tag &tag)
{
  WriteLock lock(m_mutex);
  TaskMapIt it = m_taskMap.find(tag);
  if (it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    evtimer_del(task->ev());
    m_taskMap.erase(it);
  }
}

void
Scheduler::deleteTask(const Task::TaskMatcher &matcher)
{
  WriteLock lock(m_mutex);
  TaskMapIt it = m_taskMap.begin();
  while(it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    if (matcher(task))
    {
      evtimer_del(task->ev());
      // Use post increment; map.erase invalidate the iterator that is beening erased,
      // but does not invalidate other iterators. This seems to be the convention to
      // erase something from C++ STL map while traversing.
      m_taskMap.erase(it++);
    }
    else
    {
      ++it;
    }
  }
}

int
Scheduler::size()
{
  ReadLock lock(m_mutex);
  return m_taskMap.size();
}
