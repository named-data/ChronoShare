#include "fs-watcher.h"
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/fstream.hpp>
#include <fstream>
#include <set>
#include <QtGui>
#include <iostream>

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestFsWatcherDelay)

void
onChange(const fs::path &file)
{
  cerr << "onChange called" << endl;
}

void
onDelete(const fs::path &file)
{
  cerr << "onDelete called" << endl;
}

void run(fs::path dir, FsWatcher::LocalFile_Change_Callback c, FsWatcher::LocalFile_Change_Callback d)
{
  int x = 0;
  QCoreApplication app (x, 0);
  FsWatcher watcher (dir.string().c_str(), c, d);
  app.exec();
  sleep(100);
}

void SlowWrite(fs::path & file)
{
  fs::ofstream off(file, std::ios::out);

  for (int i = 0; i < 10; i++){
    off << i  << endl;
    usleep(200000);
  }
}

BOOST_AUTO_TEST_CASE (TestFsWatcherDelay)
{
  fs::path dir = fs::absolute(fs::path("TestFsWatcher"));
  if (fs::exists(dir))
  {
    fs::remove_all(dir);
  }

  fs::create_directory(dir);

  FsWatcher::LocalFile_Change_Callback fileChange = boost::bind(onChange, _1);
  FsWatcher::LocalFile_Change_Callback fileDelete = boost::bind(onDelete, _1);

  fs::path file = dir / "test.text";

  thread watcherThread(run, dir, fileChange, fileDelete);

  thread writeThread(SlowWrite, file);



  usleep(10000000);

  // cleanup
  if (fs::exists(dir))
  {
    fs::remove_all(dir);
  }

  usleep(1000000);
}

BOOST_AUTO_TEST_SUITE_END()
