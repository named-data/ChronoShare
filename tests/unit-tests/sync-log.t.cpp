/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#include <boost/lexical_cast.hpp>
#include <boost/test/unit_test.hpp>

#include "action-log.hpp"
#include "logging.hpp"
#include <boost/filesystem.hpp>
#include <ccnx-name.hpp>
#include <iostream>
#include <unistd.h>

using namespace std;
using namespace boost;
using namespace Ndnx;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestSyncLog)


BOOST_AUTO_TEST_CASE(BasicDatabaseTest)
{
  INIT_LOGGERS();

  fs::path tmpdir = fs::unique_path(fs::temp_directory_path() / "%%%%-%%%%-%%%%-%%%%");
  SyncLog db(tmpdir, Name("/alex"));

  HashPtr hash = db.RememberStateInStateLog();
  // should be empty
  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "7a6f2c1eefd539560d2dc3e5542868a79810d0867db15d9b87e41ec105899405");

  db.UpdateDeviceSeqNo(Name("/alex"), 1);
  hash = db.RememberStateInStateLog();

  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "3410477233f98d6c3f9a6f8da24494bf5a65e1a7c9f4f66b228128bd4e020558");

  db.UpdateDeviceSeqNo(Name("/alex"), 2);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c");

  db.UpdateDeviceSeqNo(Name("/alex"), 2);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c");

  db.UpdateDeviceSeqNo(Name("/alex"), 1);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c");

  db.UpdateLocator(Name("/alex"), Name("/hawaii"));

  BOOST_CHECK_EQUAL(db.LookupLocator(Name("/alex")), Name("/hawaii"));

  SyncStateMsgPtr msg =
    db.FindStateDifferences("00", "95284d3132a7a88b85c5141ca63efa68b7a7daf37315def69e296a0c24692833");
  BOOST_CHECK_EQUAL(msg->state_size(), 0);

  msg = db.FindStateDifferences("00",
                                "2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  msg = db.FindStateDifferences("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c",
                                "00");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::DELETE);

  msg = db.FindStateDifferences("7a6f2c1eefd539560d2dc3e5542868a79810d0867db15d9b87e41ec105899405",
                                "2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  msg = db.FindStateDifferences("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c",
                                "7a6f2c1eefd539560d2dc3e5542868a79810d0867db15d9b87e41ec105899405");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 0);

  db.UpdateDeviceSeqNo(Name("/bob"), 1);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(lexical_cast<string>(*hash),
                    "5df5affc07120335089525e82ec9fda60c6dccd7addb667106fb79de80610519");

  msg = db.FindStateDifferences("00",
                                "5df5affc07120335089525e82ec9fda60c6dccd7addb667106fb79de80610519");
  BOOST_CHECK_EQUAL(msg->state_size(), 2);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  BOOST_CHECK_EQUAL(msg->state(1).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(1).seq(), 1);

  remove_all(tmpdir);
}

BOOST_AUTO_TEST_SUITE_END()
