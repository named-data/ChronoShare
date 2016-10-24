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

#include "object-db.hpp"

#include "test-common.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

class TestObjectDbFixture : public IdentityManagementTimeFixture
{
public:
  TestObjectDbFixture()
  {
    tmpdir = fs::unique_path(UNIT_TEST_CONFIG_PATH) / "TestObjectDb";
    if (exists(tmpdir)) {
      remove_all(tmpdir);
    }
  }

public:
  fs::path tmpdir;
};

BOOST_FIXTURE_TEST_SUITE(TestObjectDb, TestObjectDbFixture)

BOOST_AUTO_TEST_CASE(Basic)
{
  BOOST_CHECK_EQUAL(ObjectDb::doesExist(tmpdir, "/my-device", "abcdef"), false);

  Data data("/hello/world");
  data.setContent(Name("/some/content").wireEncode());
  m_keyChain.sign(data);

  // Saving object
  {
    ObjectDb object(tmpdir, "abcdef");
    BOOST_CHECK_EQUAL(object.getLastUsed(), time::steady_clock::now());

    BOOST_CHECK_EQUAL(ObjectDb::doesExist(tmpdir, "/my-device", "abcdef"), false);

    advanceClocks(time::hours(1));
    BOOST_CHECK_NE(object.getLastUsed(), time::steady_clock::now());

    object.saveContentObject("/my-device", 0, data);
  }

  // Retrieving object
  {
    ObjectDb object(tmpdir, "abcdef");

    BOOST_CHECK_EQUAL(ObjectDb::doesExist(tmpdir, "/my-device", "abcdef"), true);
    BOOST_CHECK(exists(tmpdir / "objects" / "ab" / "cdef"));
    BOOST_CHECK_EQUAL(object.getLastUsed(), time::steady_clock::now());

    advanceClocks(time::hours(1));
    BOOST_CHECK_NE(object.getLastUsed(), time::steady_clock::now());

    BOOST_CHECK(object.fetchSegment("/my-device", 0) != nullptr);
    auto retrievedData = object.fetchSegment("/my-device", 0);
    BOOST_REQUIRE(retrievedData != nullptr);
    BOOST_CHECK_EQUAL(*retrievedData, data);
    BOOST_CHECK_EQUAL(object.getLastUsed(), time::steady_clock::now());

    advanceClocks(time::hours(1));
    BOOST_CHECK_NE(object.getLastUsed(), time::steady_clock::now());

    BOOST_CHECK(object.fetchSegment("/my-device", 1) == nullptr);
    BOOST_CHECK_EQUAL(object.getLastUsed(), time::steady_clock::now());
  }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
