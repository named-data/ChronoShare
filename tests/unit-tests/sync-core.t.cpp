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

#include "sync-core.hpp"

#include "test-common.hpp"
#include "dummy-forwarder.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

_LOG_INIT(Test.SyncCore);

class TestSyncCoreFixture : public IdentityManagementTimeFixture
{
public:
  TestSyncCoreFixture()
    : forwarder(m_io, m_keyChain)
  {
  }

public:
  DummyForwarder forwarder;
};

BOOST_FIXTURE_TEST_SUITE(TestSyncCore, TestSyncCoreFixture)

void
callback(const SyncStateMsgPtr& msg)
{
  _LOG_DEBUG("Callback I'm called!!!!");
  BOOST_CHECK(msg->state_size() > 0);
  int size = msg->state_size();
  int index = 0;
  while (index < size) {
    SyncState state = msg->state(index);
    BOOST_CHECK(state.has_old_seq());
    if (state.seq() != 0) {
      BOOST_CHECK(state.old_seq() != state.seq());
    }
    index++;
  }
}

BOOST_AUTO_TEST_CASE(TwoNodes)
{
  fs::path tmpdir = fs::unique_path(UNIT_TEST_CONFIG_PATH) / "SyncCoreTest";
  if (exists(tmpdir)) {
    remove_all(tmpdir);
  }

  std::string dir1 = (tmpdir / "1").string();
  std::string dir2 = (tmpdir / "2").string();
  Name user1("/shuai");
  Name loc1("/locator1");
  Name user2("/loli");
  Name loc2("/locator2");
  Name syncPrefix("/broadcast/arslan");

  Face& c1 = forwarder.addFace();
  auto log1 = make_shared<SyncLog>(dir1, user1);
  auto core1 = make_shared<SyncCore>(c1, log1, user1, loc1, syncPrefix, bind(&callback, _1));

  Face& c2 = forwarder.addFace();
  auto log2 = make_shared<SyncLog>(dir2, user2);
  auto core2 = make_shared<SyncCore>(c2, log2, user2, loc2, syncPrefix, bind(&callback, _1));

  advanceClocks(time::milliseconds(10), 100);

  BOOST_CHECK_EQUAL(toHex(*core1->root()), toHex(*core2->root()));

  core1->updateLocalState(1);
  advanceClocks(time::milliseconds(10), 10);
  BOOST_CHECK_EQUAL(toHex(*core1->root()), toHex(*core2->root()));
  BOOST_CHECK_EQUAL(core2->seq(user1), 1);
  BOOST_CHECK_EQUAL(log2->LookupLocator(user1), loc1);

  core1->updateLocalState(5);
  advanceClocks(time::milliseconds(10), 10);
  BOOST_CHECK_EQUAL(toHex(*core1->root()), toHex(*core2->root()));
  BOOST_CHECK_EQUAL(core2->seq(user1), 5);
  BOOST_CHECK_EQUAL(log2->LookupLocator(user1), loc1);

  core2->updateLocalState(10);
  advanceClocks(time::milliseconds(10), 10);
  BOOST_CHECK_EQUAL(toHex(*core1->root()), toHex(*core2->root()));
  BOOST_CHECK_EQUAL(core1->seq(user2), 10);
  BOOST_CHECK_EQUAL(log1->LookupLocator(user2), loc2);

  // simple simultaneous data generation

  core1->updateLocalState(11);
  advanceClocks(time::milliseconds(10), 10);
  core2->updateLocalState(15);
  advanceClocks(time::milliseconds(10), 10);
  BOOST_CHECK_EQUAL(toHex(*core1->root()), toHex(*core2->root()));
  BOOST_CHECK_EQUAL(core1->seq(user2), 15);
  BOOST_CHECK_EQUAL(core2->seq(user1), 11);

  BOOST_CHECK_EQUAL(log1->LookupLocator(user1), loc1);
  BOOST_CHECK_EQUAL(log1->LookupLocator(user2), loc2);
  BOOST_CHECK_EQUAL(log2->LookupLocator(user1), loc1);
  BOOST_CHECK_EQUAL(log2->LookupLocator(user2), loc2);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
