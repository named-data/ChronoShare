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
#include "ccnx-wrapper.h"
#include "ccnx-common.h"
#include "scheduler.h"
#include "object-db.h"
#include "object-manager.h"
#include "content-server.h"
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <stdio.h>

using namespace Ccnx;
using namespace std;
using namespace boost;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestServeAndFetch)

path root("test-server-and-fetch");
path filePath = root / "random-file";
unsigned char magic = 'm';
int repeat = 1024 * 400;
mutex mut;
condition_variable cond;
bool finished;
int ack;

void setup()
{
  if (exists(root))
  {
    remove_all(root);
  }

  create_directory(root);

  // create file
  FILE *fp = fopen(filePath.string().c_str(), "w");
  for (int i = 0; i < repeat; i++)
  {
    fwrite(&magic, 1, sizeof(magic), fp);
  }
  fclose(fp);

  ack = 0;
  finished = false;
}

void teardown()
{
  if (exists(root))
  {
    remove_all(root);
  }

  ack = 0;
  finished = false;
}

Name
simpleMap(const Name &deviceName)
{
  return Name("/local");
}

void
segmentCallback(const Name &deviceName, const Name &baseName, uint64_t seq, PcoPtr pco)
{
  ack++;
  Bytes co = pco->content();
  int size = co.size();
  for (int i = 0; i < size; i++)
  {
    BOOST_CHECK_EQUAL(co[i], magic);
  }
}

void
finishCallback(Name &deviceName, Name &baseName)
{
  BOOST_CHECK_EQUAL(ack, repeat / 1024);
  unique_lock<mutex> lock(mut);
  finished = true;
  cond.notify_one();
}

BOOST_AUTO_TEST_CASE (TestServeAndFetch)
{
  setup();

  CcnxWrapperPtr ccnx_serve = make_shared<CcnxWrapper>();
  usleep(1000);
  CcnxWrapperPtr ccnx_fetch = make_shared<CcnxWrapper>();
  ObjectManager om(ccnx_serve, root);

  Name deviceName("/device");
  Name localPrefix("/local");
  Name broadcastPrefix("/broadcast");
  // publish file to db
  tuple<HashPtr, size_t> pub = om.localFileToObjects(filePath, deviceName);

  ActionLogPtr dummyLog;
  ContentServer server(ccnx_serve, dummyLog, root);
  server.registerPrefix(localPrefix);
  server.registerPrefix(broadcastPrefix);

  FetchManager fm(ccnx_fetch, bind(simpleMap, _1));
  HashPtr hash = get<0>(pub);
  Name baseName = Name (deviceName)("file")(hash->GetHash(), hash->GetHashBytes());
  fm.Enqueue(deviceName, baseName, bind(segmentCallback, _1, _2, _3, _4), bind(finishCallback, _1, _2), 0, get<1>(pub));

  unique_lock<mutex> lock(mut);
  system_time timeout = get_system_time() + posix_time::milliseconds(5000);
  while (!finished)
  {
    if (!cond.timed_wait(lock, timeout))
    {
      BOOST_FAIL("Fetching has not finished after 5 seconds");
      break;
    }
  }

  teardown();
}

BOOST_AUTO_TEST_SUITE_END()
