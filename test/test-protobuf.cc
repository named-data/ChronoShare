#include "ccnx-common.h"
#include "sync-core.h"
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/make_shared.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(ProtobufTests)


BOOST_AUTO_TEST_CASE (TestGzipProtobuf)
{
  SyncStateMsgPtr msg = make_shared<SyncStateMsg>();

  SyncState *state = msg->add_state();
  state->set_type(SyncState::UPDATE);
  state->set_seq(100);
  char x[100] = {'a'};
  state->set_locator(&x[0], sizeof(x));
  state->set_name(&x[0], sizeof(x));

  BytesPtr bb = serializeMsg<SyncStateMsg>(*msg);

  BytesPtr cb = serializeGZipMsg<SyncStateMsg>(*msg);
  BOOST_CHECK(cb->size() < bb->size());
  cout << cb->size() <<", " << bb->size() << endl;

  SyncStateMsgPtr msg1 = deserializeGZipMsg<SyncStateMsg>(*cb);

  BOOST_REQUIRE(msg1->state_size() == 1);

  SyncState state1 = msg1->state(0);
  BOOST_CHECK_EQUAL(state->seq(), state1.seq());
  BOOST_CHECK_EQUAL(state->type(), state1.type());
  string sx(x, 100);
  BOOST_CHECK_EQUAL(sx, state1.name());
  BOOST_CHECK_EQUAL(sx, state1.locator());
}

BOOST_AUTO_TEST_SUITE_END()
