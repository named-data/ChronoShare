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


#include <boost/test/unit_test.hpp>
#include "executor.h"

#include "logging.h"

INIT_LOGGER ("Test.Executor");

using namespace boost;
using namespace std;

void timeConsumingJob ()
{
  _LOG_DEBUG ("Start sleep");
  sleep(1);
  _LOG_DEBUG ("Finish sleep");
}

BOOST_AUTO_TEST_CASE(TestExecutor)
{
  INIT_LOGGERS ();

  {
    Executor executor (3);
    executor.start ();
    Executor::Job job = bind(timeConsumingJob);

    executor.execute(job);
    executor.execute(job);

    usleep(2000);
    // both jobs should have been taken care of
    BOOST_CHECK_EQUAL(executor.jobQueueSize(), 0);

    usleep(500000);

    // add four jobs while only one thread is idle
    executor.execute(job);
    executor.execute(job);
    executor.execute(job);
    executor.execute(job);

    usleep(1000);
    // three jobs should remain in queue
    BOOST_CHECK_EQUAL(executor.jobQueueSize(), 3);

    usleep(500000);
    // two threads should have finished and
    // take care of two queued jobs
    BOOST_CHECK_EQUAL(executor.jobQueueSize(), 1);

    // all jobs should have been fetched
    usleep(501000);
    BOOST_CHECK_EQUAL(executor.jobQueueSize(), 0);

    executor.shutdown ();
  } //separate scope to ensure that destructor is called


  sleep(1);
}
