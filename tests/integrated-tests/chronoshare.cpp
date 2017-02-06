/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */
#define BOOST_TEST_MAIN 1
#define BOOST_TEST_DYN_LINK 1
#define BOOST_TEST_MODULE ChronoShare Integrated Tests (ChronoShare)

#include "gui/chronosharegui.hpp"
#include "test-common.hpp"
#include "dummy-forwarder.hpp"

#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <thread>
#include <chrono>

namespace ndn {
namespace chronoshare {
namespace tests {

using namespace std;
namespace fs = boost::filesystem;

_LOG_INIT(Test.Integrated.ChronoShare);

class ChronoShareFixture : public IdentityManagementFixture
{
public:
  ChronoShareFixture()
    : forwarder(m_io, m_keyChain)
    , folder("sharefolder")
    , dir1(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "sharefolder1"))
    , dir2(fs::path(fs::path(UNIT_TEST_CONFIG_PATH) / "sharefolder2"))
    , argc(0)
  {

    if (fs::exists(dir1)) {
      fs::remove_all(dir1);
    }

    if (fs::exists(dir2)) {
      fs::remove_all(dir2);
    }

    fs::create_directory(dir1);
    fs::create_directory(dir2);
  }

  ~ChronoShareFixture()
  {
    //delete app;
    // cleanup
    if (fs::exists(dir1)) {
      fs::remove_all(dir1);
    }

    // cleanup
    if (fs::exists(dir2)) {
      fs::remove_all(dir2);
    }
  }

  void
  advanceClocks(std::chrono::seconds delay)
  {
    std::chrono::milliseconds step = delay;
    step /= 50;
    for (int i = 0; i < 50; ++i) {
      std::this_thread::sleep_for(step);
      m_io.poll();
      m_io.reset();
    }
  }

  void
  create_file(const fs::path& ph, const std::string& contents)
  {
    std::ofstream f(ph.string().c_str());
    if (!f) {
      abort();
    }
    if (!contents.empty()) {
      f << contents;
    }
  }

  void
  run()
  {
      std::this_thread::sleep_for(std::chrono::seconds(5));

      _LOG_DEBUG("======created files===========");
      create_file(dir1 / folder / "test.txt", "hello");

      std::this_thread::sleep_for(std::chrono::seconds(10));


      BOOST_REQUIRE_MESSAGE(fs::exists(dir2 / folder / "test.txt"), "user1 failed to notify user2 about test.txt");
      BOOST_CHECK_EQUAL(fs::file_size(dir1 / folder / "test.txt"), fs::file_size(dir2 / folder / "test.txt"));


      fs::path subdir1 = dir1 / folder / "sub";
      fs::path subdir2 = dir2 / folder / "sub";
      fs::create_directory(subdir1);
      for (int i = 0; i < 10; i++) {
        string filename = boost::lexical_cast<string>(i);
        create_file(subdir1 / filename.c_str(), boost::lexical_cast<string>(i));
      }

      std::this_thread::sleep_for(std::chrono::seconds(10));
      for (int i = 0; i < 10; i++) {
        string filename = boost::lexical_cast<string>(i);
        BOOST_REQUIRE_MESSAGE(fs::exists(subdir2 / filename), "user1 failed to notify user2 about"<< filename);
      }

      //=========check copy file to sub directory==========
      fs::create_directory(dir1 / folder / "sub1");
      fs::path subsubdir1 = dir1 / folder / "sub1" / "sub2";
      fs::path subsubdir2 = dir2 / folder / "sub1" / "sub2";
      fs::copy_directory(subdir1, subsubdir1);
      for (int i = 0; i < 5; i++) {
        string filename = boost::lexical_cast<string>(i);
        fs::copy(subdir1 / filename.c_str(), subsubdir1 / filename.c_str());
      }

      std::this_thread::sleep_for(std::chrono::seconds(10));
      // test.txt
      // sub/0..9
      // sub1/sub2/0..4
      for (int i = 0; i < 5; i++) {
        string filename = boost::lexical_cast<string>(i);
        BOOST_REQUIRE_MESSAGE(fs::exists(subsubdir2 / filename), "user1 failed to notify user2 about"<< filename);
      }

      // =============== check remove files =========================
      for (int i = 0; i < 7; i++) {
        string filename = boost::lexical_cast<string>(i);
        fs::remove(subdir1 / filename.c_str());
      }

      std::this_thread::sleep_for(std::chrono::seconds(10));
      // test.txt
      // sub/7..9
      // sub1/sub2/0..4
      for (int i = 0; i < 10; i++) {
        string filename = boost::lexical_cast<string>(i);
        if (i < 7)
          BOOST_REQUIRE_MESSAGE(!fs::exists(subdir2 / filename), "user1 failed to notify user2 about"<< filename);
        else
          BOOST_REQUIRE_MESSAGE(fs::exists(subdir2 / filename), "user1 failed to notify user2 about"<< filename);
      }

      // =================== check remove files again, remove the whole dir this time
      // ===================
      // before remove check
      for (int i = 0; i < 5; i++) {
        string filename = boost::lexical_cast<string>(i);
        BOOST_REQUIRE_MESSAGE(fs::exists(subsubdir2 / filename), "user1 failed to notify user2 about"<< filename);
      }
      fs::remove_all(subsubdir1);

      std::this_thread::sleep_for(std::chrono::seconds(10));
      // test.txt
      // sub/7..9
      BOOST_REQUIRE_MESSAGE(!fs::exists(subsubdir2), "user1 failed to notify user2 about sub1/sub2");

      // =================== check rename files =======================
      for (int i = 7; i < 10; i++) {
        string filename = boost::lexical_cast<string>(i);
        fs::rename(subdir1 / filename.c_str(), dir1 / folder / filename.c_str());
      }
      std::this_thread::sleep_for(std::chrono::seconds(10));
      // test.txt
      // 7
      // 8
      // 9
      // sub
      for (int i = 7; i < 10; i++) {
        string filename = boost::lexical_cast<string>(i);

        BOOST_REQUIRE_MESSAGE(!fs::exists(subdir2 / filename), "user1 failed to notify user2 about"<< filename);
        BOOST_REQUIRE_MESSAGE(fs::exists(dir2 / folder / filename), "user1 failed to notify user2 about"<< filename);
      }

      create_file(dir1 / folder / "add-removal-check.txt", "add-removal-check");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      fs::remove(dir1 / folder / "add-removal-check.txt");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(!fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      create_file(dir1 / folder / "add-removal-check.txt", "add-removal-check");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      fs::remove(dir1 / folder / "add-removal-check.txt");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(!fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      create_file(dir1 / folder / "add-removal-check.txt", "add-removal-check");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      fs::remove(dir1 / folder / "add-removal-check.txt");
      std::this_thread::sleep_for(std::chrono::seconds(4));
      BOOST_REQUIRE_MESSAGE(!fs::exists(dir2 / folder / "add-removal-check.txt"),
                            "user1 failed to notify user2 about add-removal-check.txt");

      _LOG_DEBUG("======finish thread===========");
  }

public:
  DummyForwarder forwarder;
  std::string folder;
  fs::path dir1;
  fs::path dir2;

  int argc;
  //TestApp* app;
};

BOOST_FIXTURE_TEST_SUITE(TestChronoShare, ChronoShareFixture)

BOOST_AUTO_TEST_CASE(Chronoshare)
{
  QApplication app(argc, nullptr);

  // do not quit when last window closes
  app.setQuitOnLastWindowClosed(false);
  // invoke gui
  ndn::chronoshare::ChronoShareGui gui1(QString::fromStdString(dir1.generic_string()),
                                        QString::fromStdString("user1"),
                                        QString::fromStdString("sharefolder"));

  // invoke gui
  ndn::chronoshare::ChronoShareGui gui2(QString::fromStdString(dir2.generic_string()),
                                        QString::fromStdString("user2"),
                                        QString::fromStdString("sharefolder"));

  QTimer::singleShot(95000, &app, SLOT(quit()));

  _LOG_DEBUG("run thread");
  std::thread workThread(boost::bind(&ChronoShareFixture::run, this));

  app.exec();
  workThread.join();
  _LOG_DEBUG("thread finished");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
