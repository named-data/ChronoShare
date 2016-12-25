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

#include "fetch-manager.hpp"
#include "ccnx-wrapper.hpp"
#include "ccnx-common.hpp"
#include "scheduler.hpp"
#include "object-db.hpp"
#include "object-manager.hpp"
#include "content-server.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread/condition_variable.hpp>
#include <stdio.h>
#include <ctime>

#include "logging.hpp"

INIT_LOGGER("Test.ServerAndFetch");

using namespace Ndnx;
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
  INIT_LOGGERS ();

  _LOG_DEBUG ("Setting up test environment ...");
  setup();

  NdnxWrapperPtr ndnx_serve = make_shared<NdnxWrapper>();
  usleep(1000);
  NdnxWrapperPtr ndnx_fetch = make_shared<NdnxWrapper>();

  Name deviceName("/test/device");
  Name localPrefix("/local");
  Name broadcastPrefix("/broadcast");

  const string APPNAME = "test-chronoshare";

  time_t start = time(NULL);
  _LOG_DEBUG ("At time " << start << ", publish local file to database, this is extremely slow ...");
  // publish file to db
  ObjectManager om(ndnx_serve, root, APPNAME);
  tuple<HashPtr, size_t> pub = om.localFileToObjects(filePath, deviceName);
  time_t end = time(NULL);
  _LOG_DEBUG ("At time " << end <<", publish finally finished, used " << end - start << " seconds ...");

  ActionLogPtr dummyLog;
  ContentServer server(ndnx_serve, dummyLog, root, deviceName, "pentagon's secrets", APPNAME, 5);
  server.registerPrefix(localPrefix);
  server.registerPrefix(broadcastPrefix);

  FetchManager fm(ndnx_fetch, bind(simpleMap, _1), Name("/local/broadcast"));
  HashPtr hash = pub.get<0> ();
  Name baseName = Name ("/")(deviceName)(APPNAME)("file")(hash->GetHash(), hash->GetHashBytes());

  fm.Enqueue(deviceName, baseName, bind(segmentCallback, _1, _2, _3, _4), bind(finishCallback, _1, _2), 0, pub.get<1>() - 1);

  unique_lock<mutex> lock(mut);
  system_time timeout = get_system_time() + posix_time::milliseconds(5000);
  while (!finished)
    {
      if (!cond.timed_wait(lock, timeout))
        {
          BOOST_FAIL ("Fetching has not finished after 5 seconds");
          break;
        }
    }
  ndnx_fetch->shutdown ();
  ndnx_serve->shutdown ();

  _LOG_DEBUG ("Finish");
  usleep(100000);

  teardown();
}

BOOST_AUTO_TEST_SUITE_END()
