#include "sync-core.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace Ccnx;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(SyncCoreTests)

typedef struct
{
  Name deviceName;
  Name locator;
  int64_t seq;
} Result;

Result result1;
Result result2;

void setResult(const SyncStateMsgPtr &msg, Result &result)
{
  if (msg->state_size() > 0)
  {
    SyncState state = msg->state(0);
    string strName = state.name();
    result.deviceName = Name((const unsigned char *)strName.c_str(), strName.size());
    string strLoc = state.locator();
    result.locator = Name((const unsigned char *)strLoc.c_str(), strLoc.size());
    result.seq = state.seq();
  }
  else
  {
    cout << "Msg state size: " << msg->state_size() << endl;
  }
}

void callback1(const SyncStateMsgPtr &msg)
{
  setResult(msg, result1);
}

void callback2(const SyncStateMsgPtr &msg)
{
  setResult(msg, result2);
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

  SyncCore *core1 = new SyncCore(dir1, user1, loc1, syncPrefix, bind(callback1, _1), c1);
  usleep(10000);
  SyncCore *core2 = new SyncCore(dir2, user2, loc2, syncPrefix, bind(callback2, _1), c2);
  usleep(1000000);

  SyncState state;

  HashPtr root1 = core1->root();
  HashPtr root2 = core2->root();
  BOOST_CHECK_EQUAL(*root1, *root2);

  cout << "\n\n\n\n\n\n----------\n";
  core1->updateLocalState(1);
  usleep(100000);
  BOOST_CHECK_EQUAL(result2.seq, 1);
  BOOST_CHECK_EQUAL(result2.deviceName, user1);
  BOOST_CHECK_EQUAL(result2.locator, loc1);

  core1->updateLocalState(5);
  usleep(100000);
  BOOST_CHECK_EQUAL(result2.seq, 5);

  core2->updateLocalState(10);
  usleep(100000);
  BOOST_CHECK_EQUAL(result1.seq, 10);
  BOOST_CHECK_EQUAL(result1.deviceName, user2);
  BOOST_CHECK_EQUAL(result1.locator, loc2);

  // simple simultaneous data generation
  cout << "\n\n\n\n\n\n----------Simultaneous\n";
  core1->updateLocalState(11);
  // change the value here 100, 2000, 3000, 5000, each with different error (for 2000 and 3000 run at least 3 times)
  usleep(100);
  core2->updateLocalState(12);
  usleep(1000000);
  BOOST_CHECK_EQUAL(result1.seq, 12);
  BOOST_CHECK_EQUAL(result2.seq, 11);

  // clean the test dir
  if (exists(d))
  {
    remove_all(d);
  }
}

BOOST_AUTO_TEST_SUITE_END()
