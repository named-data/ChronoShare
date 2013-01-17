#include "sync-core.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace Ccnx;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(SyncCoreTests)

void callback(const SyncStateMsgPtr &msg)
{
  BOOST_CHECK(msg->state_size() > 0);
}

void checkRoots(const HashPtr &root1, const HashPtr &root2)
{
  BOOST_CHECK_EQUAL(*root1, *root2);
}

BOOST_AUTO_TEST_CASE(SyncCoreTest)
{
  string dir = "./SyncCoreTest";
  string dir1 = "./SyncCoreTest/1";
  string dir2 = "./SyncCoreTest/2";
  Name user1("/joker");
  Name loc1("/gotham1");
  Name user2("/darkknight");
  Name loc2("/gotham2");
  Name syncPrefix("/broadcast/darkknight");
  CcnxWrapperPtr c1(new CcnxWrapper());
  CcnxWrapperPtr c2(new CcnxWrapper());

  // clean the test dir
  path d(dir);
  if (exists(d))
  {
    remove_all(d);
  }

  SyncCore *core1 = new SyncCore(dir1, user1, loc1, syncPrefix, bind(callback, _1), c1);
  usleep(10000);
  SyncCore *core2 = new SyncCore(dir2, user2, loc2, syncPrefix, bind(callback, _1), c2);
  usleep(1000000);
  checkRoots(core1->root(), core2->root());

  cout << "\n\n\n\n\n\n----------\n";
  core1->updateLocalState(1);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 1);

  core1->updateLocalState(5);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 5);

  core2->updateLocalState(10);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 10);

  // simple simultaneous data generation
  cout << "\n\n\n\n\n\n----------Simultaneous\n";
  core1->updateLocalState(11);
  usleep(100);
  core2->updateLocalState(15);
  usleep(1000000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 15);
  BOOST_CHECK_EQUAL(core2->seq(user1), 11);

  // clean the test dir
  if (exists(d))
  {
    remove_all(d);
  }
}

BOOST_AUTO_TEST_SUITE_END()
