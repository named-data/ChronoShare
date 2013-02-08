#include "fs-watcher.h"
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <set>
#include <QtGui>

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestFsWatcher)

void
onChange(set<string> &files, const fs::path &file)
{
  files.insert(file.string());
}

void
onDelete(set<string> &files, const fs::path &file)
{
  files.erase(file.string());
}

void create_file( const fs::path & ph, const std::string & contents )
{
  std::ofstream f( ph.string().c_str() );
  if ( !f )
  {
    abort();
  }
  if ( !contents.empty() )
  {
    f << contents;
  }
}

void run(fs::path dir, FsWatcher::LocalFile_Change_Callback c, FsWatcher::LocalFile_Change_Callback d)
{
  int x = 0;
  QCoreApplication app (x, 0);
  FsWatcher watcher (dir.string().c_str(), c, d);
  app.exec();
  sleep(100);
}

BOOST_AUTO_TEST_CASE (TestFsWatcher)
{
  fs::path dir = fs::absolute(fs::path("TestFsWatcher"));
  if (fs::exists(dir))
  {
    fs::remove_all(dir);
  }

  fs::create_directory(dir);

  set<string> files;

  FsWatcher::LocalFile_Change_Callback fileChange = boost::bind(onChange,ref(files), _1);
  FsWatcher::LocalFile_Change_Callback fileDelete = boost::bind(onDelete, ref(files), _1);

  thread workThread(run, dir, fileChange, fileDelete);
  //FsWatcher watcher (dir.string().c_str(), fileChange, fileDelete);

  // ============ check create file detection ================
  create_file(dir / "test.txt", "hello");
  // have to at least wait 0.5 seconds
  usleep(600000);
  // test.txt
  BOOST_CHECK_EQUAL(files.size(), 1);
  BOOST_CHECK(files.find("test.txt") != files.end());

  // =========== check create a bunch of files in sub dir =============
  fs::path subdir = dir / "sub";
  fs::create_directory(subdir);
  for (int i = 0; i < 10; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    create_file(subdir / filename.c_str(), boost::lexical_cast<string>(i));
  }
  // have to at least wait 0.5 * 2 seconds
  usleep(1100000);
  // test.txt
  // sub/0..9
  BOOST_CHECK_EQUAL(files.size(), 11);
  for (int i = 0; i < 10; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub/" +filename) != files.end());
  }

  // ============== check copy directory with files to two levels of sub dirs =================
  fs::create_directory(dir / "sub1");
  fs::path subdir1 = dir / "sub1" / "sub2";
  fs::copy_directory(subdir, subdir1);
  for (int i = 0; i < 5; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    fs::copy_file(subdir / filename.c_str(), subdir1 / filename.c_str());
  }
  // have to at least wait 0.5 * 2 seconds
  usleep(1100000);
  // test.txt
  // sub/0..9
  // sub1/sub2/0..4
  BOOST_CHECK_EQUAL(files.size(), 16);
  for (int i = 0; i < 5; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) != files.end());
  }

  // =============== check remove files =========================
  for (int i = 0; i < 7; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    fs::remove(subdir / filename.c_str());
  }
  usleep(1100000);
  // test.txt
  // sub/7..9
  // sub1/sub2/0..4
  BOOST_CHECK_EQUAL(files.size(), 9);
  for (int i = 0; i < 10; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    if (i < 7)
      BOOST_CHECK(files.find("sub/" + filename) == files.end());
    else
      BOOST_CHECK(files.find("sub/" + filename) != files.end());
  }

  // =================== check remove files again, remove the whole dir this time ===================
  // before remove check
  for (int i = 0; i < 5; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) != files.end());
  }
  fs::remove_all(subdir1);
  usleep(1100000);
  BOOST_CHECK_EQUAL(files.size(), 4);
  // test.txt
  // sub/7..9
  for (int i = 0; i < 5; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub1/sub2/" + filename) == files.end());
  }

  // =================== check rename files =======================
  for (int i = 7; i < 10; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    fs::rename(subdir / filename.c_str(), dir / filename.c_str());
  }
  usleep(1100000);
  // test.txt
  // 7
  // 8
  // 9
  // sub
  BOOST_CHECK_EQUAL(files.size(), 4);
  for (int i = 7; i < 10; i++)
  {
    string filename = boost::lexical_cast<string>(i);
    BOOST_CHECK(files.find("sub/" + filename) == files.end());
    BOOST_CHECK(files.find(filename) != files.end());
  }

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") != files.end());

  fs::remove (dir / "add-removal-check.txt");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") == files.end());

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") != files.end());

  fs::remove (dir / "add-removal-check.txt");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") == files.end());

  create_file(dir / "add-removal-check.txt", "add-removal-check");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") != files.end());

  fs::remove (dir / "add-removal-check.txt");
  usleep(1200000);
  BOOST_CHECK (files.find("add-removal-check.txt") == files.end());

  // cleanup
  if (fs::exists(dir))
  {
    fs::remove_all(dir);
  }
}

BOOST_AUTO_TEST_SUITE_END()
