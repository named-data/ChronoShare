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
#include "action-log.hpp"
#include "sync-core.hpp"

#include "test-common.hpp"
#include "dummy-forwarder.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

_LOG_INIT(Test.ContentServer);

class TestContentServerFixture : public IdentityManagementTimeFixture
{
public:
  TestContentServerFixture()
    : forwarder(m_io, m_keyChain)
    , face(forwarder.addFace())
    , appName("test-chronoshare")
    , deviceName("/device")
    , shareFolderName("sharefolder")
    , root("test-server-and-fetch")
  {
    cleanDir(root);

    create_directory(root);

    syncLog = make_shared<SyncLog>(root, deviceName);
    actionLog = std::make_shared<ActionLog>(face, root, syncLog,
                                            "top-secret", "test-chronoshare",
                                            ActionLog::OnFileAddedOrChangedCallback(),
                                            ActionLog::OnFileRemovedCallback());

    actionLog->AddLocalActionUpdate("file.txt",
                                    *fromHex("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                                    std::time(nullptr), 0755, 10);
    BOOST_CHECK_EQUAL(syncLog->SeqNo(deviceName), 1);

    ndn::ConstBufferPtr hash = syncLog->RememberStateInStateLog();

    server = make_unique<ContentServer>(face, actionLog, root,
                                        deviceName, shareFolderName,
                                        "test-chronoshare", m_keyChain, 5);

    Name localPrefix("/local");
    Name broadcastPrefix("/multicast");

    server->registerPrefix(localPrefix);
    server->registerPrefix(broadcastPrefix);

    advanceClocks(time::milliseconds(10), 1000);
  }

  ~TestContentServerFixture()
  {
    cleanDir(root);
  }

  void
  onActionData(const Interest& interest, Data& data)
  {
    _LOG_DEBUG("on action data, interest Name: " << interest);
  }

  void
  onTimeout(const Interest& interest)
  {
    _LOG_DEBUG("on timeout, interest Name: " << interest);
    BOOST_CHECK(false);
  }

  void cleanDir(fs::path dir) {
    if (exists(dir)) {
      remove_all(dir);
    }
  }

public:
  DummyForwarder forwarder;
  Face& face;
  shared_ptr<SyncLog> syncLog;
  shared_ptr<ActionLog> actionLog;
  unique_ptr<ContentServer> server;
  Name appName;
  Name deviceName;

  std::string shareFolderName;

  fs::path root;
};

BOOST_FIXTURE_TEST_SUITE(TestContentServer, TestContentServerFixture)

BOOST_AUTO_TEST_CASE(TestContentServerServe)
{
  Interest interest(
    Name("/local").append(deviceName).append(appName).append("action")
                  .append(shareFolderName).appendNumber(1));

  _LOG_DEBUG("interest Name: " << interest);

  dynamic_cast<util::DummyClientFace*>(&face)->receive(interest);

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),1);

  Data data = dynamic_cast<util::DummyClientFace*>(&face)->sentData.at(0);

  BOOST_CHECK_EQUAL(data.getName(), "/local/device/test-chronoshare/action/sharefolder/%01");

  shared_ptr<ActionItem> action = deserializeMsg<ActionItem>(
    Buffer(data.getContent().value(), data.getContent().value_size()));

  BOOST_CHECK_EQUAL(action->action(), 0);
  BOOST_CHECK_EQUAL(action->filename(), "file.txt");
  BOOST_CHECK_EQUAL(action->file_hash().size(), 32);
  BOOST_CHECK_EQUAL(action->version(), 0);
  BOOST_CHECK_EQUAL(action->has_parent_device_name(), false);
  BOOST_CHECK_EQUAL(action->has_parent_seq_no(), false);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
