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

#include "logging.h"
#include "object-manager.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/test/unit_test.hpp>
#include <unistd.h>
#include <boost/make_shared.hpp>
#include <iostream>
#include <iterator>

INIT_LOGGER ("Test.ObjectManager");

using namespace Ccnx;
using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestObjectManager)

BOOST_AUTO_TEST_CASE (ObjectManagerTest)
{
  INIT_LOGGERS ();

  fs::path tmpdir = fs::unique_path (fs::temp_directory_path () / "%%%%-%%%%-%%%%-%%%%");
  _LOG_DEBUG ("tmpdir: " << tmpdir);
  Name deviceName ("/device");

  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();
  ObjectManager manager (ccnx, tmpdir);

  tuple<HashPtr,int> hash_semgents = manager.localFileToObjects (fs::path("test") / "test-object-manager.cc", deviceName);

  BOOST_CHECK_EQUAL (hash_semgents.get<1> (), 3);

  bool ok = manager.objectsToLocalFile (deviceName, *hash_semgents.get<0> (), tmpdir / "test.cc");
  BOOST_CHECK_EQUAL (ok, true);

  {
    fs::ifstream origFile (fs::path("test") / "test-object-manager.cc");
    fs::ifstream newFile (tmpdir / "test.cc");

    istream_iterator<char> eof,
      origFileI (origFile),
      newFileI (newFile);

    BOOST_CHECK_EQUAL_COLLECTIONS (origFileI, eof, newFileI, eof);
  }

  remove_all (tmpdir);
}


BOOST_AUTO_TEST_SUITE_END()
