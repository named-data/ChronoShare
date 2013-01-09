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

#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

#include <unistd.h>
#include "action-log.h"
#include <iostream>
#include <ccnx-name.h>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;
using namespace Ccnx;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(DatabaseTest)


BOOST_AUTO_TEST_CASE (BasicDatabaseTest)
{
  fs::path tmpdir = fs::unique_path (fs::temp_directory_path () / "%%%%-%%%%-%%%%-%%%%");
  SyncLog db (tmpdir, "/alex");

  HashPtr hash = db.RememberStateInStateLog ();
  // should be empty
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "7a6f2c1eefd539560d2dc3e5542868a79810d0867db15d9b87e41ec105899405");

  db.UpdateDeviceSeqno (Name ("Alex"), 1);
  hash = db.RememberStateInStateLog ();

  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "bf19308cb2c2ddab7bcce66e9074cd0eed74893be0813ca67a95e97c55d65896");

  db.UpdateDeviceSeqno (Name ("Alex"), 2);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0");

  db.UpdateDeviceSeqno (Name ("Alex"), 2);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0");

  db.UpdateDeviceSeqno (Name ("Alex"), 1);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0");

  
  // db.FindStateDifferences ("00", "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");
  // db.FindStateDifferences ("86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0", "00");
  // db.FindStateDifferences ("869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce",
  //                          "86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0");
  // db.FindStateDifferences ("86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0",
  //                          "869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce");

  // db.UpdateDeviceSeqno (Name ("Bob"), 1);
  // hash = db.RememberStateInStateLog ();
  // BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");

  // db.FindStateDifferences ("00", "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");
  // db.FindStateDifferences ("86b51f1f98662583b295b61385ae4450ff8fac955981f1ca4381144ab1d7a4e0",
  //                          "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");

  remove_all (tmpdir);
}

BOOST_AUTO_TEST_SUITE_END()
