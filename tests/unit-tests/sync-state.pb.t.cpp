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

#include "sync-state.pb.h"
#include "sync-core.hpp"

#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include "test-common.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

BOOST_AUTO_TEST_SUITE(TestSyncStatePb)

BOOST_AUTO_TEST_CASE(Serialize)
{
  auto msg = make_shared<SyncStateMsg>();

  SyncState* state = msg->add_state();
  state->set_type(SyncState::UPDATE);
  state->set_seq(100);
  char x[100] = {'a'};
  state->set_locator(&x[0], sizeof(x));
  state->set_name(&x[0], sizeof(x));

  ndn::ConstBufferPtr bb = serializeMsg<SyncStateMsg>(*msg);

  ndn::ConstBufferPtr cb = serializeGZipMsg<SyncStateMsg>(*msg);
  BOOST_CHECK_LT(cb->size(), bb->size());

  SyncStateMsgPtr msg1 = deserializeGZipMsg<SyncStateMsg>(*cb);

  BOOST_REQUIRE_EQUAL(msg1->state_size(), 1);

  SyncState state1 = msg1->state(0);
  BOOST_CHECK_EQUAL(state->seq(), state1.seq());
  BOOST_CHECK_EQUAL(state->type(), state1.type());

  std::string sx(x, 100);
  BOOST_CHECK_EQUAL(sx, state1.name());
  BOOST_CHECK_EQUAL(sx, state1.locator());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
