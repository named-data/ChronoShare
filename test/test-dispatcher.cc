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
 * Author:  Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *          Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ccnx-wrapper.h"
#include "logging.h"
#include "dispatcher.h"
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <cassert>

using namespace Ccnx;
using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

INIT_LOGGER ("Test.Dispatcher");

BOOST_AUTO_TEST_SUITE(DispatcherTest)


void cleanDir(fs::path dir)
{
  if (fs::exists(dir))
  {
    fs::remove_all(dir);
  }
}

void checkRoots(const HashPtr &root1, const HashPtr &root2)
{
  BOOST_CHECK_EQUAL(*root1, *root2);
}

BOOST_AUTO_TEST_CASE(TestDispatcher)
{
  INIT_LOGGERS ();

  fs::path dir1("./TestDispatcher/test-white-house");
  fs::path dir2("./TestDispatcher/test-black-house");

  string user1 = "/obamaa";
  string user2 = "/romney";

  string folder = "who-is-president";

  CcnxWrapperPtr ccnx1 = make_shared<CcnxWrapper>();
  usleep(100);
  CcnxWrapperPtr ccnx2 = make_shared<CcnxWrapper>();
  usleep(100);

  cleanDir(dir1);
  cleanDir(dir2);

  Dispatcher d1(user1, folder, dir1, ccnx1, 2, false);
  usleep(100);
  Dispatcher d2(user2, folder, dir2, ccnx2, 2, false);

  usleep(1000000);

  _LOG_DEBUG ("checking obama vs romney");
  checkRoots(d1.SyncRoot(), d2.SyncRoot());

  fs::path filename("a_letter_to_romney.txt");
  string words = "I'm the new socialist President. You are not!";

  fs::path abf = dir1 / filename;

  ofstream ofs;
  ofs.open(abf.string().c_str());
  ofs << words;
  ofs.close();

  d1.Did_LocalFile_AddOrModify(filename);

  sleep(2);

  fs::path ef = dir2 / filename;
  BOOST_REQUIRE_MESSAGE(fs::exists(ef), user1 << " failed to notify " << user2 << " about " << filename.string());
  BOOST_CHECK_EQUAL(fs::file_size(abf), fs::file_size(ef));
  HashPtr fileHash1 = Hash::FromFileContent(abf);
  HashPtr fileHash2 = Hash::FromFileContent(ef);
  BOOST_CHECK_EQUAL(*fileHash1, *fileHash2);

  // cleanDir(dir1);
  // cleanDir(dir2);
}

BOOST_AUTO_TEST_SUITE_END()
