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

using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(DatabaseTest)


BOOST_AUTO_TEST_CASE (BasicDatabaseTest)
{
  char dir_tmpl [] = "/tmp/tmp-chornoshare-XXXXXXXXXXX";
  string tmp_dir = mkdtemp (dir_tmpl);
  SyncLog db (tmp_dir, "/alex");

  HashPtr hash = db.RememberStateInStateLog ();
  // should be empty
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "461f0ed1300b7f947fbe8e38a04186b74938febe7e43fe4ed571551fa3bd6ab9");

  db.UpdateDeviceSeqno ("Alex", 1);
  hash = db.RememberStateInStateLog ();

  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "80463c859f23367e1cbabfa80d6de78af334589ec88dc9c56c854c1f7e196c34");

  db.UpdateDeviceSeqno ("Alex", 2);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");

  db.UpdateDeviceSeqno ("Alex", 2);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");

  db.UpdateDeviceSeqno ("Alex", 1);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");

  
  db.FindStateDifferences ("00", "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");
  db.FindStateDifferences ("95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833", "00");
  db.FindStateDifferences ("869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce",
                           "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");
  db.FindStateDifferences ("95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833",
                           "869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce");

  db.UpdateDeviceSeqno ("Bob", 1);
  hash = db.RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");

  db.FindStateDifferences ("00", "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");
  db.FindStateDifferences ("95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833",
                           "d001d4680fd9adcb48e34a795e3cc3d5d36f209fbab34fd57f70f362c2085310");

}

BOOST_AUTO_TEST_SUITE_END()
