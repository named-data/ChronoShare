#include "sync-core.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace Ccnx;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(SyncCoreTests)

SyncStateMsgPtr msg1;
SyncStateMsgPtr msg2;

void callback1(const SyncStateMsgPtr &ptr)
{
  msg1 = ptr;
}

void callback2(const SyncStateMsgPtr &ptr)
{
  msg2 = ptr;
}

BOOST_AUTO_TEST_CASE(SyncCoreTest)
{
  string dir = "./SyncCoreTest";
  Name user1("/joker");
  Name loc1("/gotham1");
  Name user2("/darkknight");
  Name loc2("/gotham2");
  Name syncPrefix("/broadcast/darkknight");
  CcnxWrapperPtr c1(new CcnxWrapper());
  CcnxWrapperPtr c2(new CcnxWrapper());
  SchedulerPtr scheduler(new Scheduler());
  scheduler->start();

  // clean the test dir
  path d(dir);
  if (exists(d))
  {
    remove_all(d);
  }

  SyncCore *core1 = new SyncCore(dir, user1, loc1, syncPrefix, bind(callback1, _1), c1, scheduler);
  usleep(10000);
  SyncCore *core2 = new SyncCore(dir, user2, loc2, syncPrefix, bind(callback2, _1), c2, scheduler);
  usleep(10000);

  SyncState state;

  core1->updateLocalState(1);
  usleep(100000);
  BOOST_CHECK_EQUAL(msg2->state_size(), 1);
  state = msg2->state(0);
  BOOST_CHECK_EQUAL(state.seq(), 1);
  BOOST_CHECK_EQUAL(user1, state.name());
  BOOST_CHECK_EQUAL(loc1, state.locator());

  core1->updateLocalState(5);
  usleep(100000);
  state = msg2->state(0);
  BOOST_CHECK_EQUAL(state.seq(), 5);

  core2->updateLocalState(10);
  usleep(100000);
  state = msg1->state(0);
  BOOST_CHECK_EQUAL(state.seq(), 10);

  // simple simultaneous data generation
  core1->updateLocalState(11);
  core2->updateLocalState(12);
  usleep(100000);
  state = msg1->state(0);
  BOOST_CHECK_EQUAL(state.seq(), 12);
  state = msg2->state(0);
  BOOST_CHECK_EQUAL(state.seq(), 11);

  // clean the test dir
  if (exists(d))
  {
    remove_all(d);
  }
}

BOOST_AUTO_TEST_SUITE_END()
