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

#include "state-server.hpp"
#include "sync-core.hpp"
#include "object-manager.hpp"
#include "action-log.hpp"

#include "test-common.hpp"
#include <ndn-cxx/util/dummy-client-face.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::property_tree::ptree;
using boost::property_tree::read_json;

namespace ndn {
namespace chronoshare {
namespace tests {

namespace fs = boost::filesystem;

_LOG_INIT(Test.StateServer);

class TestStateServerFixture : public IdentityManagementTimeFixture
{
public:
  TestStateServerFixture()
    //: face(m_io, m_keyChain, {true, true})
    : localName("/yukai")
    , tmpdir(fs::unique_path(UNIT_TEST_CONFIG_PATH))
    , shareFolderName("sharefolder")
    , root("test-server-and-fetch")
    , filename("file.txt")
    , abf(root / filename)
    , face(m_io, m_keyChain, {true, true})
  {
    if (exists(root)) {
      remove_all(root);
    }

    syncLog = make_shared<SyncLog>(root, localName);

    std::string words = "this is the test msg";


    manager = make_unique<ObjectManager>(face, m_keyChain, root, "test-chronoshare");
    actionLog = std::make_shared<ActionLog>(face, root, syncLog,
                                                 "top-secret", name::Component("test-chronoshare"),
                                                 ActionLog::OnFileAddedOrChangedCallback(),
                                                 bind([&] { remove(abf); }));

    server = std::make_shared<StateServer>(face, actionLog, root, localName,
                                           shareFolderName, name::Component("test-chronoshare"),
                                           *manager, m_keyChain, time::seconds(5));

    std::ofstream ofs;
    ofs.open(abf.string().c_str());
    ofs << words;
    ofs.close();

    fileSize = fs::file_size(abf);

    _LOG_DEBUG("absolutePath: " << abf << " localUserName: " << localName);
    tie(hash, seg_num) = manager->localFileToObjects(abf, localName);

    BOOST_REQUIRE_MESSAGE(fs::exists(abf), " failed to create the file");

    actionLog->AddLocalActionUpdate(filename.generic_string(), *hash,
                                    std::time(nullptr), 0644, seg_num);

    actionLog->AddLocalActionUpdate("sharefolder/file.txt",
                                    *fromHex("2ff3ab223a23c2435519eef13daabf8576dceffc62618a431715aaf6eea2bf1c"),
                                    std::time(nullptr), 0755, 10);

    syncLog->RememberStateInStateLog();

    actionLog->AddLocalActionDelete("file.txt");

    BOOST_REQUIRE_MESSAGE(!fs::exists(abf), " failed to delete the file");

    syncLog->RememberStateInStateLog();
  }

  ~TestStateServerFixture()
  {
    if (exists(root)) {
      remove_all(root);
    }
  }

public:
  //util::DummyClientFace& face;
  Name localName;
  fs::path tmpdir;

  std::string shareFolderName;
  fs::path root;
  fs::path filename;
  fs::path abf;

  util::DummyClientFace face;

  shared_ptr<SyncLog> syncLog;
  shared_ptr<ActionLog> actionLog;
  shared_ptr<StateServer> server;
  unique_ptr<ObjectManager> manager;

  int seg_num;
  ConstBufferPtr hash;

  uintmax_t fileSize;
};

BOOST_FIXTURE_TEST_SUITE(TestStateServer, TestStateServerFixture)

BOOST_AUTO_TEST_CASE(TestStateServer)
{
  Interest interest1(
    Name("/localhop").append(localName).append("test-chronoshare").append(shareFolderName)
                     .append("info").append("files").append("folder").appendNumber(0));

  _LOG_DEBUG("interest1:"<<interest1.toUri());

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),1);
  dynamic_cast<util::DummyClientFace*>(&face)->receive(interest1);

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),2);

  Data data1 = dynamic_cast<util::DummyClientFace*>(&face)->sentData.at(1);

  BOOST_CHECK_EQUAL(data1.getName(),
                    "/localhop/yukai/test-chronoshare/sharefolder/info/files/folder/%00");

  // Read json.
  ptree pt1;
  std::istringstream is1(std::string(data1.getContent().value(),
                                     data1.getContent().value() + data1.getContent().value_size()));
  read_json(is1, pt1);
  for (ptree::value_type &file : pt1.get_child("files"))
  {
      BOOST_CHECK_EQUAL(file.second.get<std::string>("filename"), "sharefolder/file.txt");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("version"), "0");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("owner.userName"), "/yukai");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("owner.seqNo"), "2");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("hash"),
        "2FF3AB223A23C2435519EEF13DAABF8576DCEFFC62618A431715AAF6EEA2BF1C");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("chmod"), "0755");
      BOOST_CHECK_EQUAL(file.second.get<std::string>("segNum"), "10");
  }

  // <PREFIX_INFO>/"actions"/"folder|file"/<folder|file>/<offset>  get list of all actions
  Interest interest2(
    Name("/localhop").append(localName).append("test-chronoshare")
                     .append(shareFolderName).append("info").append("actions")
                     .append("file").append("file.txt").appendNumber(0));

  _LOG_DEBUG("interest2:"<<interest2.toUri());
  dynamic_cast<util::DummyClientFace*>(&face)->receive(interest2);

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),3);

  Data data2 = dynamic_cast<util::DummyClientFace*>(&face)->sentData.at(2);

  BOOST_CHECK_EQUAL(data2.getName(),
                    "/localhop/yukai/test-chronoshare/sharefolder/info/actions/file/file.txt/%00");

  // Read json.
  ptree pt2;
  std::istringstream is2(std::string(data2.getContent().value(),
                                     data2.getContent().value() + data2.getContent().value_size()));
  read_json(is2, pt2);
  for (ptree::value_type &action : pt2.get_child("actions"))
  {
    BOOST_CHECK_EQUAL(action.second.get<std::string>("id.userName"), "/yukai");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("filename"), "file.txt");
    if(action.second.get<std::string>("version") == "1")
    {
      BOOST_CHECK_EQUAL(action.second.get<std::string>("id.seqNo"), "3");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("version"), "1");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("action"), "DELETE");
    }
    else {
      BOOST_CHECK_EQUAL(action.second.get<std::string>("id.seqNo"), "1");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("version"), "0");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("action"), "UPDATE");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("update.hash"),
        "FC0D526AF0BA965EA6BCD5E5BE281B0F5497BBAC2850DFA7C04D31D383E02D0B");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("update.chmod"), "0644");
      BOOST_CHECK_EQUAL(action.second.get<std::string>("update.segNum"), "1");

    }
  }

  Interest interest3(
    Name("/localhop").append(localName).append("test-chronoshare")
                     .append(shareFolderName).append("info").append("actions")
                     .append("folder").append("sharefolder").appendNumber(0));

  _LOG_DEBUG("interest3:"<<interest3.toUri());
  dynamic_cast<util::DummyClientFace*>(&face)->receive(interest3);

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),4);

  Data data3 = dynamic_cast<util::DummyClientFace*>(&face)->sentData.at(3);

  BOOST_CHECK_EQUAL(data3.getName(),
                    "/localhop/yukai/test-chronoshare/sharefolder/info/actions/folder/sharefolder/%00");

  ptree pt3;
  std::istringstream is3(std::string(data3.getContent().value(),
                                     data3.getContent().value() + data3.getContent().value_size()));
  read_json(is3, pt3);
  //std::cout<<"get files"<<pt2.get<std::string>("files..")<<std::endl;
  for (ptree::value_type &action : pt3.get_child("actions"))
  {
    BOOST_CHECK_EQUAL(action.second.get<std::string>("id.userName"), "/yukai");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("id.seqNo"), "2");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("filename"), "sharefolder/file.txt");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("version"), "0");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("action"), "UPDATE");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("update.hash"),
      "2FF3AB223A23C2435519EEF13DAABF8576DCEFFC62618A431715AAF6EEA2BF1C");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("update.chmod"), "0755");
    BOOST_CHECK_EQUAL(action.second.get<std::string>("update.segNum"), "10");
  }

  Interest interest4(
    Name("/localhop").append(localName).append("test-chronoshare").append(shareFolderName)
                     .append("cmd").append("restore").append("file").append("file.txt")
                     .appendNumber(0).append(name::Component(hash)));

  dynamic_cast<util::DummyClientFace*>(&face)->receive(interest4);

  advanceClocks(time::milliseconds(10), 1000);

  BOOST_CHECK_EQUAL(dynamic_cast<util::DummyClientFace*>(&face)->sentData.size(),5);

  Data data4 = dynamic_cast<util::DummyClientFace*>(&face)->sentData.at(4);

  _LOG_DEBUG(std::string(data4.getContent().value(),
                         data4.getContent().value()+data4.getContent().value_size()));

  BOOST_REQUIRE_MESSAGE(fs::exists(abf), " failed to restore the file");
  BOOST_CHECK_EQUAL(fileSize, fs::file_size(abf));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn