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

#include "action-log.hpp"

#include "test-common.hpp"
#include "dummy-forwarder.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

class TestActionLogFixture : public IdentityManagementTimeFixture
{
public:
  TestActionLogFixture()
    : forwarder(m_io, m_keyChain)
    , localName("/lijing")
    , tmpdir(fs::unique_path(UNIT_TEST_CONFIG_PATH))
  {
    if (exists(tmpdir)) {
      remove_all(tmpdir);
    }

    syncLog = make_shared<SyncLog>(tmpdir, localName);
  }

public:
  DummyForwarder forwarder;
  Name localName;
  fs::path tmpdir;
  shared_ptr<SyncLog> syncLog;
};

BOOST_FIXTURE_TEST_SUITE(TestActionLog, TestActionLogFixture)

BOOST_AUTO_TEST_CASE(UpdateAction)
{
  auto actionLog = std::make_shared<ActionLog>(forwarder.addFace(), tmpdir, syncLog,
                                               "top-secret", "test-chronoshare",
                                               ActionLog::OnFileAddedOrChangedCallback(),
                                               ActionLog::OnFileRemovedCallback());

  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 0);

  BOOST_CHECK_EQUAL(syncLog->LogSize(), 0);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 0);

  actionLog->AddLocalActionUpdate("file.txt",
                                  *fromHex("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                                  std::time(nullptr), 0755, 10);

  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 1);
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 0);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 1);

  ndn::ConstBufferPtr hash = syncLog->RememberStateInStateLog();
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 1);
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "91A849EEDE75ACD56AE1BCB99E92D8FB28757683BC387DBB0E59C3108FCF4F18");

  ndn::shared_ptr<ndn::Data> data = actionLog->LookupActionData(localName, 0);

  BOOST_CHECK_EQUAL(bool(data), false);

  data = actionLog->LookupActionData(localName, 1);

  BOOST_CHECK_EQUAL(bool(data), true);

  BOOST_CHECK_EQUAL(data->getName(), "/lijing/test-chronoshare/action/top-secret/%01");

  ActionItemPtr action =
    actionLog->LookupAction(Name("/lijing/test-chronoshare/action/top-secret").appendNumber(0));
  BOOST_CHECK_EQUAL((bool)action, false);

  action =
    actionLog->LookupAction(Name("/lijing/test-chronoshare/action/top-secret").appendNumber(1));
  BOOST_CHECK_EQUAL((bool)action, true);

  if (action) {
    BOOST_CHECK_EQUAL(action->version(), 0);
    BOOST_CHECK_EQUAL(action->action(), ActionItem::UPDATE);

    BOOST_CHECK_EQUAL(action->filename(), "file.txt");
    BOOST_CHECK_EQUAL(action->seg_num(), 10);
    BOOST_CHECK_EQUAL(action->file_hash().size(), 32);
    BOOST_CHECK_EQUAL(action->mode(), 0755);

    BOOST_CHECK_EQUAL(action->has_parent_device_name(), false);
    BOOST_CHECK_EQUAL(action->has_parent_seq_no(), false);
  }

  actionLog->AddLocalActionUpdate("file.txt",
                                  *fromHex("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                                  std::time(nullptr), 0755, 10);
  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 2);
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 1);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 2);

  action = actionLog->LookupAction(Name("/lijing"), 2);
  BOOST_CHECK_EQUAL((bool)action, true);

  if (action) {
    BOOST_CHECK_EQUAL(action->has_parent_device_name(), true);
    BOOST_CHECK_EQUAL(action->has_parent_seq_no(), true);

    BOOST_CHECK_EQUAL(action->parent_seq_no(), 1);
    BOOST_CHECK_EQUAL(action->version(), 1);
  }

  BOOST_CHECK_EQUAL((bool)actionLog->AddRemoteAction(data), true);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 2);

  // create a real remote action
  ActionItemPtr item = make_shared<ActionItem>();
  item->set_action(ActionItem::UPDATE);
  item->set_filename("file.txt");
  item->set_version(2);
  item->set_timestamp(std::time(nullptr));

  std::string item_msg;
  item->SerializeToString(&item_msg);

  Name actionName = Name("/zhenkai/test/test-chronoshare/action/top-secret").appendNumber(1);

  ndn::shared_ptr<Data> actionData = ndn::make_shared<Data>();
  actionData->setName(actionName);
  actionData->setContent(reinterpret_cast<const uint8_t*>(item_msg.c_str()), item_msg.size());
  ndn::KeyChain m_keyChain;
  m_keyChain.sign(*actionData);

  BOOST_CHECK_EQUAL((bool)actionLog->AddRemoteAction(actionData), true);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 3);
}

BOOST_AUTO_TEST_CASE(DeleteAction)
{
  bool hasDeleteCallbackCalled = false;
  auto actionLog = std::make_shared<ActionLog>(forwarder.addFace(), tmpdir, syncLog,
                                               "top-secret", "test-chronoshare",
                                               ActionLog::OnFileAddedOrChangedCallback(),
                                               bind([&] { hasDeleteCallbackCalled = true; }));

  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 0);

  BOOST_CHECK_EQUAL(syncLog->LogSize(), 0);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 0);

  actionLog->AddLocalActionUpdate("file.txt",
                                  *fromHex("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                                  std::time(nullptr), 0755, 10);

  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 1);
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 0);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 1);

  syncLog->RememberStateInStateLog();

  actionLog->AddLocalActionDelete("file.txt");
  BOOST_CHECK_EQUAL(syncLog->SeqNo(localName), 2);
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 1);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 2);
  BOOST_CHECK(hasDeleteCallbackCalled);

  ndn::ConstBufferPtr hash = syncLog->RememberStateInStateLog();
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 2);
  BOOST_CHECK_EQUAL(toHex(*hash),
                    "D2DFEDA56ED98C0E17D455A859BC8C3B9E31C85C138C280A8BADAB4FC551F282");

  ndn::shared_ptr<ndn::Data> data = actionLog->LookupActionData(localName, 2);
  BOOST_CHECK(data != nullptr);

  BOOST_CHECK_EQUAL(data->getName(), "/lijing/test-chronoshare/action/top-secret/%02");

  ActionItemPtr action = actionLog->LookupAction(Name("/lijing/test-chronoshare/action/top-secret").appendNumber(2));
  BOOST_CHECK(action != nullptr);

  if (action) {
    BOOST_CHECK_EQUAL(action->version(), 1);
    BOOST_CHECK_EQUAL(action->action(), ActionItem::DELETE);

    BOOST_CHECK_EQUAL(action->filename(), "file.txt");

    BOOST_CHECK_EQUAL(Name(Block(action->parent_device_name().data(), action->parent_device_name().size())),
                      Name("/lijing"));
    BOOST_CHECK_EQUAL(action->parent_seq_no(), 1);
  }

  BOOST_CHECK_EQUAL((bool)actionLog->AddRemoteAction(data), true);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 2);

  // create a real remote action
  ActionItemPtr item = make_shared<ActionItem>();
  item->set_action(ActionItem::DELETE);
  item->set_filename("file.txt");
  item->set_version(2);
  item->set_timestamp(std::time(0));

  std::string filename = "file.txt";
  BufferPtr parent_device_name = std::make_shared<Buffer>(filename.c_str(), filename.size());
  item->set_parent_device_name(parent_device_name->buf(), parent_device_name->size());
  item->set_parent_seq_no(0);

  std::string item_msg;
  item->SerializeToString(&item_msg);

  Name actionName = Name("/yukai/test/test-chronoshare/action/top-secret").appendNumber(1);

  ndn::shared_ptr<Data> actionData = ndn::make_shared<Data>();
  actionData->setName(actionName);
  actionData->setContent(reinterpret_cast<const uint8_t*>(item_msg.c_str()), item_msg.size());
  ndn::KeyChain m_keyChain;
  m_keyChain.sign(*actionData);

  ActionItemPtr actionItem = actionLog->AddRemoteAction(actionData);
  BOOST_CHECK_EQUAL((bool)actionItem, true);
  BOOST_CHECK_EQUAL(actionLog->LogSize(), 3);
  BOOST_CHECK_EQUAL(syncLog->LogSize(), 2);

  BOOST_CHECK_EQUAL(action->action(), ActionItem::DELETE);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
