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

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <boost/function.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <deque>

/* A very simple executor to execute submitted tasks immediately or 
 * in the future (depending on whether there is idle thread)
 * A fixed number of threads are created for executing tasks;
 * The policy is FIFO
 * No cancellation of submitted tasks
 */

class Executor
{
public:
  typedef boost::function<void ()> Job;

  Executor(int poolSize);
  ~Executor();

  // execute the job immediately or sometime in the future
  void
  execute(const Job &job);

  int
  poolSize();

// only for test
  int
  jobQueueSize();

private:
  void
  run();

  Job
  waitForJob();

private:
  typedef std::deque<Job> JobQueue;
  typedef boost::mutex Mutex;
  typedef boost::unique_lock<Mutex> Lock;
  typedef boost::condition_variable Cond;
  typedef boost::thread Thread;
  typedef boost::thread_group ThreadGroup;
  JobQueue m_queue;
  Mutex m_mutex;
  Cond m_cond;
  ThreadGroup m_group;
};
#endif // EXECUTOR_H
