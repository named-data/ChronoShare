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
#include "dispatcher.hpp"
#include "dummy-forwarder.hpp"
#include "test-common.hpp"

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <cassert>
#include <fstream>

namespace fs = boost::filesystem;

namespace ndn {
namespace chronoshare {
namespace tests {

_LOG_INIT(Test.Dispatcher);

class TestDispatcherFixture : public IdentityManagementTimeFixture
{
public:
  TestDispatcherFixture()
    : forwarder(m_io, m_keyChain)
    , dir1(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "TestDispatcher/test-white-house"))
    , dir2(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "TestDispatcher/test-black-house"))
    , user1("/obamaa")
    , user2("/trump")
    , folder("who-is-president")
    , face1(forwarder.addFace())
    , face2(forwarder.addFace())
  {
  }

  ~TestDispatcherFixture()
  {
    if (exists(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "TestDispatcher"))) {
      remove_all(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "TestDispatcher"));
    }
  }

  void
  checkRoots(ndn::ConstBufferPtr root1, ndn::ConstBufferPtr root2)
  {
  }

public:
  DummyForwarder forwarder;
  fs::path dir1;
  fs::path dir2;
  std::string user1;
  std::string user2;
  std::string folder;
  Face& face1;
  Face& face2;
};

BOOST_FIXTURE_TEST_SUITE(TestDispatcher, TestDispatcherFixture)

BOOST_AUTO_TEST_CASE(DispatcherTest)
{
  Dispatcher d1(user1, folder, dir1, face1);
  Dispatcher d2(user2, folder, dir2, face2);

  advanceClocks(time::milliseconds(10), 1000);

  _LOG_DEBUG("checking obama vs trump");
  BOOST_CHECK(*(d1.SyncRoot()) == *(d2.SyncRoot()));

  fs::path filename("a_letter_to_obama.txt");
  std::string words = "I'm the new socialist President. You are not!";

  fs::path abf = dir1 / filename;

  std::ofstream ofs;
  ofs.open(abf.string().c_str());
  for (int i = 0; i < 5000; i++) {
    ofs << words;
  }
  ofs.close();

  d1.Did_LocalFile_AddOrModify(filename);

  advanceClocks(time::milliseconds(10), 1000);

  fs::path ef = dir2 / filename;
  BOOST_REQUIRE_MESSAGE(fs::exists(ef), user1 << " failed to notify " << user2 << " about "
                                              << filename.string());
  BOOST_CHECK_EQUAL(fs::file_size(abf), fs::file_size(ef));

  ConstBufferPtr fileHash1 = digestFromFile(abf);
  ConstBufferPtr fileHash2 = digestFromFile(ef);

  BOOST_CHECK(*fileHash1 == *fileHash2);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
