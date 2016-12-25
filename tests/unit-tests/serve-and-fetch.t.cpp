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

#include "content-server.hpp"
#include "fetch-manager.hpp"
#include "object-db.hpp"
#include "object-manager.hpp"
#include "dummy-forwarder.hpp"

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_time.hpp>
#include <ctime>
#include <stdio.h>

#include "test-common.hpp"


using namespace std;
using namespace boost;
using namespace boost::filesystem;


namespace ndn {
namespace chronoshare {
namespace tests {

_LOG_INIT(Test.ServerAndFetch);

class TestServerAndFetchFixture : public IdentityManagementTimeFixture
{
public:
  TestServerAndFetchFixture()
    : forwarder(m_io, m_keyChain)
    , root("test-server-and-fetch")
    , filePath(root / "random-file")
    , character('m')
    , repeat(1024 * 40)
    , finished(false)
    , ack(0)
  {
    if (exists(root)) {
      remove_all(root);
    }

    create_directory(root);

    // create file
    FILE* fp = fopen(filePath.string().c_str(), "w");
    for (int i = 0; i < repeat; i++) {
      fwrite(&character, 1, sizeof(character), fp);
    }
    fclose(fp);

    ack = 0;
    finished = false;
  }

  ~TestServerAndFetchFixture()
  {
    if (exists(root)) {
      remove_all(root);
    }
  }

  Name
  simpleMap(const Name& deviceName)
  {
    return Name("/local");
  }

  void
  segmentCallback(const Name& deviceName, const Name& baseName, uint64_t seq,
                  ndn::shared_ptr<ndn::Data> data)
  {
    ack++;
    ndn::Block block = Data(data->getContent().blockFromValue()).getContent();
    int size = block.value_size();
    const uint8_t* co = block.value();
    for (int i = 0; i < size; i++) {
      BOOST_CHECK_EQUAL(co[i], character);
    }

    _LOG_DEBUG("I'm called!!! segmentCallback!! size " << size << " ack times:" << ack);
  }

  void
  finishCallback(Name& deviceName, Name& baseName)
  {
    BOOST_CHECK_EQUAL(ack, repeat / 1024);
    finished = true;
    cond.notify_one();
  }

public:
  DummyForwarder forwarder;
  path root;
  path filePath;

  unsigned char character;
  int repeat;

  boost::condition_variable cond;
  bool finished;
  int ack;
};

BOOST_FIXTURE_TEST_SUITE(TestServeAndFetch, TestServerAndFetchFixture)

BOOST_AUTO_TEST_CASE(TestServeAndFetch)
{
  Face& face_serve = forwarder.addFace();
  Face& face_fetch = forwarder.addFace();

  Name deviceName("/test/device");
  Name localPrefix("/local");
  Name broadcastPrefix("/multicast");

  const string APPNAME = "test-chronoshare";

  time_t start = std::time(NULL);
  _LOG_DEBUG("At time " << start << ", publish local file to database, this is extremely slow ...");

  // publish file to db
  ObjectManager om(face_serve, m_keyChain, root, APPNAME);
  auto pub = om.localFileToObjects(filePath, deviceName);

  time_t end = std::time(NULL);
  _LOG_DEBUG("At time " << end << ", publish finally finished, used " << end - start
                        << " seconds ...");

  ActionLogPtr dummyLog;
  ContentServer server(face_serve, dummyLog, root, deviceName, "pentagon's secrets",
                       name::Component(APPNAME), m_keyChain, time::seconds(5));
  server.registerPrefix(localPrefix);
  server.registerPrefix(broadcastPrefix);

  FetchManager fm(face_fetch, bind(&TestServerAndFetchFixture::simpleMap, this, _1), Name("/"));
  ConstBufferPtr hash = std::get<0>(pub);

  Name baseName = Name(deviceName);
  baseName.append(APPNAME).append("file").appendImplicitSha256Digest(hash);

  fm.Enqueue(deviceName, baseName, bind(&TestServerAndFetchFixture::segmentCallback, this, _1, _2, _3, _4),
             bind(&TestServerAndFetchFixture::finishCallback, this, _1, _2), 0, std::get<1>(pub) - 1);

  advanceClocks(time::milliseconds(10), 1000);

  _LOG_DEBUG("Finish");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
