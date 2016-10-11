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

#include "io-service-manager.hpp"
#include "core/logging.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

namespace ndn {
namespace chronoshare {

using namespace ndn::chronoshare;

_LOG_INIT(FaceService);

IoServiceManager::IoServiceManager(boost::asio::io_service& io)
  : m_ioService(io)
  , m_connect(true)
{
}

IoServiceManager::~IoServiceManager()
{
  handle_stop();
}

void
IoServiceManager::run()
{
  while (m_connect) {
    try {
      m_ioServiceWork.reset(new boost::asio::io_service::work(m_ioService));
      m_ioService.reset();
      m_ioService.run();
    }
    catch (...) {
      _LOG_DEBUG("error while connecting to the forwarder");
    }
  }
}

void
IoServiceManager::handle_stop()
{
  m_connect = false;
  m_ioService.stop();
}

} // namespace chronoshare
} // namespace ndn
