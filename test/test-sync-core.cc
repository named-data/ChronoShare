#include "sync-core.h"
#include "logging.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

using namespace std;
using namespace Ccnx;
using namespace boost;
using namespace boost::filesystem;

INIT_LOGGER("Test.SyncCore");

BOOST_AUTO_TEST_SUITE(SyncCoreTests)

void callback(const SyncStateMsgPtr &msg)
{
  BOOST_CHECK(msg->state_size() > 0);
  int size = msg->state_size();
  int index = 0;
  while (index < size)
  {
    SyncState state = msg->state(index);
    BOOST_CHECK(state.has_old_seq());
    BOOST_CHECK(state.old_seq() >= 0);
    if (state.seq() != 0)
    {
      BOOST_CHECK(state.old_seq() != state.seq());
    }
    index++;
  }
}

void checkRoots(const HashPtr &root1, const HashPtr &root2)
{
  BOOST_CHECK_EQUAL(*root1, *root2);
}

BOOST_AUTO_TEST_CASE(SyncCoreTest)
{
  INIT_LOGGERS();

  string dir = "./SyncCoreTest";
  // clean the test dir
  path d(dir);
  if (exists(d))
  {
    remove_all(d);
  }

  string dir1 = "./SyncCoreTest/1";
  string dir2 = "./SyncCoreTest/2";
  Name user1("/joker");
  Name loc1("/gotham1");
  Name user2("/darkknight");
  Name loc2("/gotham2");
  Name syncPrefix("/broadcast/darkknight");
  CcnxWrapperPtr c1(new CcnxWrapper());
  CcnxWrapperPtr c2(new CcnxWrapper());
  SyncLogPtr log1(new SyncLog(dir1, user1.toString()));
  SyncLogPtr log2(new SyncLog(dir2, user2.toString()));

  SyncCore *core1 = new SyncCore(log1, user1, loc1, syncPrefix, bind(callback, _1), c1);
  usleep(10000);
  SyncCore *core2 = new SyncCore(log2, user2, loc2, syncPrefix, bind(callback, _1), c2);

  sleep(1);
  checkRoots(core1->root(), core2->root());

  // _LOG_TRACE ("\n\n\n\n\n\n----------\n");

  core1->updateLocalState(1);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 1);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user1), loc1);

  core1->updateLocalState(5);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 5);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user1), loc1);

  core2->updateLocalState(10);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 10);
  BOOST_CHECK_EQUAL(log1->LookupLocator (user2), loc2);

  // simple simultaneous data generation
  // _LOG_TRACE ("\n\n\n\n\n\n----------Simultaneous\n");
  _LOG_TRACE ("Simultaneous");

  core1->updateLocalState(11);
  usleep(100);
  core2->updateLocalState(15);
  usleep(2000000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 15);
  BOOST_CHECK_EQUAL(core2->seq(user1), 11);

  BOOST_CHECK_EQUAL(log1->LookupLocator (user1), loc1);
  BOOST_CHECK_EQUAL(log1->LookupLocator (user2), loc2);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user1), loc1);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user2), loc2);

  // clean the test dir
  if (exists(d))
  {
    remove_all(d);
  }
}

BOOST_AUTO_TEST_SUITE_END()
