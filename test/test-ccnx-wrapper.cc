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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ccnx-wrapper.h"
#include "ccnx-closure.h"
#include "ccnx-name.h"
#include "ccnx-selectors.h"
#include "ccnx-pco.h"
#include <unistd.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(TestCcnxWrapper)

CcnxWrapperPtr c1;
CcnxWrapperPtr c2;
int g_timeout_counter = 0;
int g_dataCallback_counter = 0;

void publish1(const Name &name)
{
  string content = name.toString();
  c1->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void publish2(const Name &name)
{
  string content = name.toString();
  c2->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void dataCallback(const Name &name, Ccnx::PcoPtr pco)
{
  BytesPtr content = pco->contentPtr ();
  string msg(reinterpret_cast<const char *> (head (*content)), content->size());
  g_dataCallback_counter ++;
  BOOST_CHECK_EQUAL(name, msg);
}

void
timeout(const Name &name, const Closure &closure, Selectors selectors)
{
  g_timeout_counter ++;
}

BOOST_AUTO_TEST_CASE (BlaCcnxWrapperTest)
{
  c1 = make_shared<CcnxWrapper> ();
  c2 = make_shared<CcnxWrapper> ();

  Name prefix1("/c1");
  Name prefix2("/c2");

  c1->setInterestFilter(prefix1, bind(publish1, _1));
  usleep(100000);
  c2->setInterestFilter(prefix2, bind(publish2, _1));

  Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1, _2, _3));

  c1->sendInterest(Name("/c2/hi"), closure);
  usleep(100000);
  c2->sendInterest(Name("/c1/hi"), closure);
  sleep(1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 2);
  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
}

BOOST_AUTO_TEST_CASE (CcnxWrapperSelector)
{

  Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1, _2, _3));

  Selectors selectors;
  selectors.interestLifetime(1);

  string n1 = "/random/01";
  c1->sendInterest(Name(n1), closure, selectors);
  sleep(2);
  c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 0);

  string n2 = "/random/02";
  selectors.interestLifetime(2);
  c1->sendInterest(Name(n2), closure, selectors);
  sleep(1);
  c2->publishData(Name(n2), (const unsigned char *)n2.c_str(), n2.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);

  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
}

void
reexpress(const Name &name, const Closure &closure, Selectors selectors)
{
  g_timeout_counter ++;
  c1->sendInterest(name, closure, selectors);
}

BOOST_AUTO_TEST_CASE (TestTimeout)
{
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
  Closure closure (bind(dataCallback, _1, _2), bind(reexpress, _1, _2, _3));

  Selectors selectors;
  selectors.interestLifetime(1);

  string n1 = "/random/04";
  c1->sendInterest(Name(n1), closure, selectors);
  usleep(3500000);
  c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 1);
  usleep(1000);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);
  BOOST_CHECK_EQUAL(g_timeout_counter, 3);
}
// BOOST_AUTO_TEST_CASE (CcnxWrapperSigningTest)
// {
//   Bytes data;
//   data.resize(1024);
//   for (int i = 0; i < 1024; i++)
//   {
//     data[i] = 'm';
//   }

//   Name name("/signingtest");

//   posix_time::ptime start = posix_time::second_clock::local_time();
//   for (uint64_t i = 0; i < 10000; i++)
//   {
//     Name n = name;
//     n.appendComp(i);
//     c1->publishData(n, data, 10);
//   }
//   posix_time::ptime end = posix_time::second_clock::local_time();

//   posix_time::time_duration duration = end - start;

//   cout << "Publishing 10000 1K size content objects costs " <<duration.total_milliseconds() << " milliseconds" << endl;
//   cout << "Average time to publish one content object is " << (double) duration.total_milliseconds() / 10000.0 << " milliseconds" << endl;
// }

BOOST_AUTO_TEST_SUITE_END()
