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
#include "fetch-manager.hpp"

#include "test-common.hpp"

#include <ndn-cxx/util/dummy-client-face.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace ndn {
namespace chronoshare {
namespace tests {

_LOG_INIT(Test.FetchManager);

BOOST_AUTO_TEST_SUITE(TestFetchManager)

class FetcherTestData : public IdentityManagementTimeFixture
{
public:
  FetcherTestData()
    : face(m_io, m_keyChain, {true, true})
    , m_done(false)
    , m_failed(false)
    , m_hasMissing(true)
  {
  }

  void
  onData(const ndn::Name& deviceName, const ndn::Name& basename, uint64_t seqno,
         ndn::shared_ptr<ndn::Data> data)
  {
    _LOG_TRACE("onData: " << seqno << data->getName());

    recvData.insert(seqno);
    differentNames.insert(basename);
    Name name = basename;
    name.appendNumber(seqno);
    segmentNames.insert(name);

    Block block = data->getContent();
    std::string recvNo(reinterpret_cast<const char*>(block.value()), block.value_size());
    recvContent.insert(boost::lexical_cast<uint32_t>(recvNo));
  }

  void
  finish(const ndn::Name& deviceName, const ndn::Name& baseName)
  {
  }

  void
  onComplete(Fetcher& fetcher)
  {
    m_done = true;
  }

  void
  onFail(Fetcher& fetcher)
  {
    m_failed = true;
  }

public:
  util::DummyClientFace face;
  std::set<uint32_t> recvData;
  std::set<uint32_t> recvContent;

  std::set<Name> differentNames;
  std::set<Name> segmentNames;

  bool m_done;
  bool m_failed;

  bool m_hasMissing;
};

BOOST_FIXTURE_TEST_CASE(TestFetcher, FetcherTestData)
{
  Name baseName("/fetchtest");
  Name deviceName("/device");

  // publish seqnos:  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, <gap 5>, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, <gap 1>, 26
  // this will allow us to test our pipeline of 6
  std::set<uint32_t> seqs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, /*<gap 10-14>,*/
                             15, 16, 17, 18, 19, 20, 21, 22, 23, 24, /*<gap 25>,*/ 26};

  face.onSendInterest.connect([&, this] (const Interest& interest) {

      uint32_t requestedSeqNo = interest.getName().at(-1).toNumber();
      if (seqs.count(requestedSeqNo) > 0) {

        auto data = make_shared<Data>();
        Name name(baseName);
        name.appendNumber(requestedSeqNo);

        data->setName(name);
        data->setFreshnessPeriod(time::seconds(300));
        std::string content = to_string(requestedSeqNo);
        data->setContent(reinterpret_cast<const uint8_t*>(content.data()), content.size());
        m_keyChain.sign(*data);
        m_io.post([data, this] { face.receive(*data); });
      }
    });

  Fetcher fetcher(face, bind(&FetcherTestData::onData, this, _1, _2, _3, _4),
                  bind(&FetcherTestData::finish, this, _1, _2),
                  bind(&FetcherTestData::onComplete, this, _1),
                  bind(&FetcherTestData::onFail, this, _1), deviceName, Name("/fetchtest"), 0, 26,
                  time::seconds(5), Name());

  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  this->advanceClocks(time::milliseconds(50), 1000);
  BOOST_CHECK_EQUAL(m_failed, true);
  BOOST_CHECK_EQUAL(differentNames.size(), 1);
  BOOST_CHECK_EQUAL(segmentNames.size(), 20);
  BOOST_CHECK_EQUAL(recvData.size(), 20);
  BOOST_CHECK_EQUAL(recvContent.size(), 20);

  BOOST_CHECK_EQUAL("0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24",
                    join(recvData | boost::adaptors::transformed([] (int i) { return std::to_string(i); }), ", "));
  BOOST_CHECK_EQUAL_COLLECTIONS(recvData.begin(), recvData.end(), recvContent.begin(), recvContent.end());

  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  this->advanceClocks(time::milliseconds(100), 100);
  BOOST_CHECK_EQUAL(m_done, false);
  BOOST_CHECK_EQUAL(m_failed, true);
  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);

  std::set<uint32_t> missing = {10, 11, 12, 13, 14, 25};
  seqs.insert(missing.begin(), missing.end());

  m_failed = false;
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  this->advanceClocks(time::milliseconds(100), 100);
  BOOST_CHECK_EQUAL(m_done, true);
  BOOST_CHECK_EQUAL(m_failed, false);
  BOOST_CHECK_EQUAL(segmentNames.size(), 27);
  BOOST_CHECK_EQUAL(recvData.size(), 27);
  BOOST_CHECK_EQUAL(recvContent.size(), 27);

  BOOST_CHECK_EQUAL("0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26",
                    join(recvData | boost::adaptors::transformed([] (int i) { return std::to_string(i); }), ", "));
  BOOST_CHECK_EQUAL_COLLECTIONS(recvData.begin(), recvData.end(), recvContent.begin(), recvContent.end());

  // TODO add tests that other callbacks got called
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
