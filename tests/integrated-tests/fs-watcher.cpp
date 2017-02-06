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
#define BOOST_TEST_MAIN 1
#define BOOST_TEST_DYN_LINK 1
#define BOOST_TEST_MODULE ChronoShare Integrated Tests (FsWatcher)

#include "fs-watcher.hpp"
#include "test-common.hpp"

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include <set>
#include <QtWidgets>

using namespace std;
namespace fs = boost::filesystem;

_LOG_INIT(Test.FSWatcher);

namespace ndn {
namespace chronoshare {
namespace tests {

class TestFSWatcherFixture : public IdentityManagementFixture
{
public:
  TestFSWatcherFixture()
    : dir(fs::path(UNIT_TEST_CONFIG_PATH) / "TestFsWatcher")
    , argc(0)
  {
    if (fs::exists(dir)) {
      fs::remove_all(dir);
    }

    fs::create_directory(dir);
  }

  ~TestFSWatcherFixture(){
    // cleanup
    if (fs::exists(dir)) {
      _LOG_DEBUG("clean all");
      fs::remove_all(dir);
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
  onChange(set<string>& files, const fs::path& file)
  {
    _LOG_DEBUG("on change, file: " << file);
    files.insert(file.string());
  }

  void
  onDelete(set<string>& files, const fs::path& file)
  {
    _LOG_DEBUG("on delete, file: " << file);
    files.erase(file.string());
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
    QApplication app(argc, nullptr);
    new FsWatcher(m_io, dir.string().c_str(),
                  std::bind(&TestFSWatcherFixture::onChange, this, std::ref(files), _1),
                  std::bind(&TestFSWatcherFixture::onDelete, this, std::ref(files), _1),
                  &app);

    QTimer::singleShot(21000, &app, SLOT(quit()));
    app.exec();
  }

public:
  fs::path dir;
  set<string> files;
  int argc;
  //fsWatcherApp* app;
};

BOOST_FIXTURE_TEST_SUITE(TestFsWatcher, TestFSWatcherFixture)

BOOST_AUTO_TEST_CASE(TestFsWatcher)
{
  std::thread workThread(boost::bind(&TestFSWatcherFixture::run, this));
  this->advanceClocks(std::chrono::seconds(2));

  // ============ check create file detection ================
  create_file(dir / "test.txt", "hello");
  this->advanceClocks(std::chrono::seconds(2));
  // test.txt
  BOOST_CHECK_EQUAL(files.size(), 1);
  BOOST_CHECK(files.find("test.txt") != files.end());

  // =========== check create a bunch of files in sub dir =============
  fs::path subdir = dir / "sub";
  fs::create_directory(subdir);
  for (int i = 0; i < 10; i++) {
    string filename = boost::lexical_cast<string>(i);
    create_file(subdir / filename.c_str(), boost::lexical_cast<string>(i));
  }
  this->advanceClocks(std::chrono::seconds(2));
  // test.txt
  // sub/0..9
  BOOST_CHECK_EQUAL(files.size(), 11);
  for (int i = 0; i < 10; i++) {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub/" + filename) != files.end());
  }

  // ============== check copy directory with files to two levels of sub dirs =================
  fs::create_directory(dir / "sub1");
  fs::path subdir1 = dir / "sub1" / "sub2";
  fs::copy_directory(subdir, subdir1);
  for (int i = 0; i < 5; i++) {
    string filename = boost::lexical_cast<string>(i);
    fs::copy(subdir / filename.c_str(), subdir1 / filename.c_str());
  }
  this->advanceClocks(std::chrono::seconds(2));
  // test.txt
  // sub/0..9
  // sub1/sub2/0..4
  BOOST_CHECK_EQUAL(files.size(), 16);
  for (int i = 0; i < 5; i++) {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) != files.end());
  }

  // =============== check remove files =========================
  for (int i = 0; i < 7; i++) {
    string filename = boost::lexical_cast<string>(i);
    fs::remove(subdir / filename.c_str());
  }
  this->advanceClocks(std::chrono::seconds(2));
  // test.txt
  // sub/7..9
  // sub1/sub2/0..4
  BOOST_CHECK_EQUAL(files.size(), 9);
  for (int i = 0; i < 10; i++) {
    string filename = boost::lexical_cast<string>(i);
    if (i < 7)
      BOOST_CHECK(files.find("sub/" + filename) == files.end());
    else
      BOOST_CHECK(files.find("sub/" + filename) != files.end());
  }

  // =================== check remove files again, remove the whole dir this time
  // ===================
  // before remove check
  for (int i = 0; i < 5; i++) {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) != files.end());
  }
  fs::remove_all(subdir1);
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK_EQUAL(files.size(), 4);
  // test.txt
  // sub/7..9
  for (int i = 0; i < 5; i++) {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) == files.end());
  }

  // =================== check rename files =======================
  for (int i = 7; i < 10; i++) {
    string filename = boost::lexical_cast<string>(i);
    fs::rename(subdir / filename.c_str(), dir / filename.c_str());
  }
  this->advanceClocks(std::chrono::seconds(2));
  // test.txt
  // 7
  // 8
  // 9
  // sub
  BOOST_CHECK_EQUAL(files.size(), 4);
  for (int i = 7; i < 10; i++) {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub/" + filename) == files.end());
    BOOST_CHECK(files.find(filename) != files.end());
  }

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") != files.end());

  fs::remove(dir / "add-removal-check.txt");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") == files.end());

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") != files.end());

  fs::remove(dir / "add-removal-check.txt");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") == files.end());

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") != files.end());

  fs::remove(dir / "add-removal-check.txt");
  this->advanceClocks(std::chrono::seconds(2));
  BOOST_CHECK(files.find("add-removal-check.txt") == files.end());

  //emit app->stopApp();

  workThread.join();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
