/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
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

#include "sync-log.hpp"

#include "test-common.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

INIT_LOGGER("Test.SyncLog")

BOOST_FIXTURE_TEST_SUITE(TestSyncLog, IdentityManagementTimeFixture)

BOOST_AUTO_TEST_CASE(BasicDatabaseTest)
{
  fs::path tmpdir = fs::unique_path(UNIT_TEST_CONFIG_PATH);
  if (exists(tmpdir)) {
    remove_all(tmpdir);
  }

  SyncLog db(tmpdir, Name("/lijing"));

  ndn::ConstBufferPtr hash = db.RememberStateInStateLog();
  // should be empty

  BOOST_CHECK_EQUAL(toHex(*hash),
                    "94D988A90C6A3D0F74624368BE65E5369DDDDB3444841FAD4EF41F674B937F26");

  db.UpdateDeviceSeqNo(Name("/lijing"), 1);
  hash = db.RememberStateInStateLog();

  BOOST_CHECK_EQUAL(toHex(*hash),
                    "91A849EEDE75ACD56AE1BCB99E92D8FB28757683BC387DBB0E59C3108FCF4F18");

  db.UpdateDeviceSeqNo(Name("/lijing"), 2);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");

  db.UpdateDeviceSeqNo(Name("/lijing"), 2);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");

  db.UpdateDeviceSeqNo(Name("/lijing"), 1);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");

  db.UpdateLocator(Name("/lijing"), Name("/hawaii"));

  BOOST_CHECK_EQUAL(db.LookupLocator(Name("/lijing")), Name("/hawaii"));

  SyncStateMsgPtr msg = db.FindStateDifferences("00", "95284D3132A7A88B85C5141CA63EFA68B7A7DAF37315DEF69E296A0C24692833");
  BOOST_CHECK_EQUAL(msg->state_size(), 0);

  msg = db.FindStateDifferences("00",
                                "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  msg = db.FindStateDifferences("D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282",
                                "00");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::DELETE);

  msg = db.FindStateDifferences("94D988A90C6A3D0F74624368BE65E5369DDDDB3444841FAD4EF41F674B937F26",
                                "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  msg = db.FindStateDifferences("D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282",
                                "94D988A90C6A3D0F74624368BE65E5369DDDDB3444841FAD4EF41F674B937F26");
  BOOST_CHECK_EQUAL(msg->state_size(), 1);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 0);

  db.UpdateDeviceSeqNo(Name("/shuai"), 1);
  hash = db.RememberStateInStateLog();
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "602FF1878FC394B90E4A0E90C7409EA4B8EE8AA40169801D62F838470551DB7C");

  msg = db.FindStateDifferences("00",
                                "602FF1878FC394B90E4A0E90C7409EA4B8EE8AA40169801D62F838470551DB7C");
  BOOST_CHECK_EQUAL(msg->state_size(), 2);
  BOOST_CHECK_EQUAL(msg->state(0).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(0).seq(), 2);

  BOOST_CHECK_EQUAL(msg->state(1).type(), SyncState::UPDATE);
  BOOST_CHECK_EQUAL(msg->state(1).seq(), 1);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
