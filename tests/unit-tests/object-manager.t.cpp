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

#include "object-manager.hpp"

#include "test-common.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

_LOG_INIT(Test.ObjectManager);

class TestObjectManagerFixture : public IdentityManagementTimeFixture
{
public:
  TestObjectManagerFixture()
    : face(m_io, m_keyChain, {true, true})
  {
    tmpdir = fs::unique_path(UNIT_TEST_CONFIG_PATH) / "TestObjectManager";
    if (exists(tmpdir)) {
      remove_all(tmpdir);
    }
    manager = make_unique<ObjectManager>(face, m_keyChain, tmpdir, "test-chronoshare");
  }

public:
  util::DummyClientFace face;
  unique_ptr<ObjectManager> manager;
  fs::path tmpdir;
};

BOOST_FIXTURE_TEST_SUITE(TestObjectManager, TestObjectManagerFixture)

BOOST_AUTO_TEST_CASE(FromFileToFile)
{
  Name deviceName("/device");

  auto objects = manager->localFileToObjects(fs::path("tests") / "unit-tests" / "object-manager.t.cpp", deviceName);

  BOOST_CHECK_EQUAL(std::get<1>(objects), 3);

  bool ok = manager->objectsToLocalFile(deviceName, *std::get<0>(objects), tmpdir / "test.cpp");
  BOOST_CHECK_EQUAL(ok, true);

  {
    fs::ifstream origFile(fs::path("tests") / "unit-tests" / "object-manager.t.cpp");
    fs::ifstream newFile(tmpdir / "test.cpp");

    std::istream_iterator<char> eof, origFileI(origFile), newFileI(newFile);

    BOOST_CHECK_EQUAL_COLLECTIONS(origFileI, eof, newFileI, eof);
  }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
