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
#include "fetcher.h"
#include "ccnx-wrapper.h"
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include "logging.h"


INIT_LOGGER ("Test.FetchManager");

using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(TestFetchManager)

struct FetcherTestData
{
  set<uint64_t> recvData;
  set<uint64_t> recvContent;

  set<Name> differentNames;
  set<Name> segmentNames;

  bool m_done;
  bool m_failed;

  FetcherTestData ()
    : m_done (false)
    , m_failed (false)
  {
  }

  void
  onData (const Ccnx::Name &deviceName, const Ccnx::Name &basename, uint64_t seqno, Ccnx::PcoPtr pco)
  {
    _LOG_TRACE ("onData: " << seqno);

    recvData.insert (seqno);
    differentNames.insert (basename);
    Name name = basename;
    name.appendComp(seqno);
    segmentNames.insert (name);

    BytesPtr data = pco->contentPtr ();

    if (data->size () == sizeof(int))
      {
        recvContent.insert (*reinterpret_cast<const int*> (head(*data)));
      }

    // cout << "<<< " << basename << ", " << name << ", " << seqno << endl;
  }

  void
  finish(const Ccnx::Name &deviceName, const Ccnx::Name &baseName)
  {
  }

  void
  onComplete (Fetcher &fetcher)
  {
    m_done = true;
    // cout << "Done" << endl;
  }

  void
  onFail (Fetcher &fetcher)
  {
    m_failed = true;
    // cout << "Failed" << endl;
  }
};

void run()
{
  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();

  Name baseName ("/base");
  Name deviceName ("/device");

  for (int i = 0; i < 10; i++)
    {
      usleep(100000);
      ccnx->publishData (Name (baseName)(i), reinterpret_cast<const unsigned char*> (&i), sizeof(int), 30);
    }

  for (int i = 11; i < 50; i++)
    {
      usleep(100000);
      ccnx->publishData (Name (baseName)(i), reinterpret_cast<const unsigned char*> (&i), sizeof(int), 30);
    }

}

BOOST_AUTO_TEST_CASE (TestFetcher)
{
  INIT_LOGGERS ();

  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();

  Name baseName ("/base");
  Name deviceName ("/device");
  /* publish seqnos:  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, <gap 5>, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, <gap 1>, 26 */
  // this will allow us to test our pipeline of 6
  for (int i = 0; i < 10; i++)
    {
      ccnx->publishData (Name (baseName)(i), reinterpret_cast<const unsigned char*> (&i), sizeof(int), 30);
    }

  for (int i = 15; i < 25; i++)
    {
      ccnx->publishData (Name (baseName)(i), reinterpret_cast<const unsigned char*> (&i), sizeof(int), 30);
    }

  int oneMore = 26;
  ccnx->publishData (Name (baseName)(oneMore), reinterpret_cast<const unsigned char*> (&oneMore), sizeof(int), 30);

  FetcherTestData data;
  ExecutorPtr executor = make_shared<Executor>(1);
  executor->start ();

  Fetcher fetcher (ccnx,
                   executor,
                   bind (&FetcherTestData::onData, &data, _1, _2, _3, _4),
                   bind (&FetcherTestData::finish, &data, _1, _2),
                   bind (&FetcherTestData::onComplete, &data, _1),
                   bind (&FetcherTestData::onFail, &data, _1),
                   deviceName, Name ("/base"), 0, 26,
                   boost::posix_time::seconds (5)); // this time is not precise

  BOOST_CHECK_EQUAL (fetcher.IsActive (), false);
  fetcher.RestartPipeline ();
  BOOST_CHECK_EQUAL (fetcher.IsActive (), true);

  usleep(7000000);
  BOOST_CHECK_EQUAL (data.m_failed, true);
  BOOST_CHECK_EQUAL (data.differentNames.size (), 1);
  BOOST_CHECK_EQUAL (data.segmentNames.size (), 20);
  BOOST_CHECK_EQUAL (data.recvData.size (), 20);
  BOOST_CHECK_EQUAL (data.recvContent.size (), 20);

  {
    ostringstream recvData;
    for (set<uint64_t>::iterator i = data.recvData.begin (); i != data.recvData.end (); i++)
      recvData << *i << ", ";

    ostringstream recvContent;
    for (set<uint64_t>::iterator i = data.recvContent.begin (); i != data.recvContent.end (); i++)
      recvContent << *i << ", ";

    BOOST_CHECK_EQUAL (recvData.str (), "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, ");
    BOOST_CHECK_EQUAL (recvData.str (), recvContent.str ());
  }

  BOOST_CHECK_EQUAL (fetcher.IsActive (), false);
  fetcher.RestartPipeline ();
  BOOST_CHECK_EQUAL (fetcher.IsActive (), true);

  usleep(7000000);
  BOOST_CHECK_EQUAL (data.m_failed, true);

  // publishing missing pieces
  for (int i = 0; i < 27; i++)
    {
      ccnx->publishData (Name (baseName)(i), reinterpret_cast<const unsigned char*> (&i), sizeof(int), 1);
    }
  BOOST_CHECK_EQUAL (fetcher.IsActive (), false);
  fetcher.RestartPipeline ();
  BOOST_CHECK_EQUAL (fetcher.IsActive (), true);

  usleep(1000000);
  BOOST_CHECK_EQUAL (data.m_done, true);

  {
    ostringstream recvData;
    for (set<uint64_t>::iterator i = data.recvData.begin (); i != data.recvData.end (); i++)
      recvData << *i << ", ";

    ostringstream recvContent;
    for (set<uint64_t>::iterator i = data.recvContent.begin (); i != data.recvContent.end (); i++)
      recvContent << *i << ", ";

    BOOST_CHECK_EQUAL (recvData.str (), "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, ");
    BOOST_CHECK_EQUAL (recvData.str (), recvContent.str ());
  }

  executor->shutdown ();
}


BOOST_AUTO_TEST_CASE (TestFetcher2)
{
  INIT_LOGGERS ();

  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();

  Name baseName ("/base");
  Name deviceName ("/device");

  thread publishThread(run);

  FetcherTestData data;
  ExecutorPtr executor = make_shared<Executor>(1);
  executor->start ();

  Fetcher fetcher (ccnx,
                   executor,
                   bind (&FetcherTestData::onData, &data, _1, _2, _3, _4),
                   bind (&FetcherTestData::finish, &data, _1, _2),
                   bind (&FetcherTestData::onComplete, &data, _1),
                   bind (&FetcherTestData::onFail, &data, _1),
                   deviceName, baseName, 0, 49,
                   boost::posix_time::seconds (5)); // this time is not precise

  BOOST_CHECK_EQUAL (fetcher.IsActive (), false);
  fetcher.RestartPipeline ();
  BOOST_CHECK_EQUAL (fetcher.IsActive (), true);

  usleep(20000000);
  BOOST_CHECK_EQUAL (data.m_failed, true);

  executor->shutdown ();
}



// BOOST_AUTO_TEST_CASE (CcnxWrapperSelector)
// {

//   Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1));

//   Selectors selectors;
//   selectors.interestLifetime(1);

//   string n1 = "/random/01";
//   c1->sendInterest(Name(n1), closure, selectors);
//   sleep(2);
//   c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 4);
//   usleep(100000);
//   BOOST_CHECK_EQUAL(g_timeout_counter, 1);
//   BOOST_CHECK_EQUAL(g_dataCallback_counter, 0);

//   string n2 = "/random/02";
//   selectors.interestLifetime(2);
//   c1->sendInterest(Name(n2), closure, selectors);
//   sleep(1);
//   c2->publishData(Name(n2), (const unsigned char *)n2.c_str(), n2.size(), 4);
//   usleep(100000);
//   BOOST_CHECK_EQUAL(g_timeout_counter, 1);
//   BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);

//   // reset
//   g_dataCallback_counter = 0;
//   g_timeout_counter = 0;
// }

BOOST_AUTO_TEST_SUITE_END()
