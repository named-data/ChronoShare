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

#ifndef CHRONOSHARE_SRC_IO_SERVICE_MANAGER_HPP
#define CHRONOSHARE_SRC_IO_SERVICE_MANAGER_HPP

#include <ndn-cxx/face.hpp>
#include <boost/asio/io_service.hpp>

#define RECONNECTION_TIME 30000

namespace ndn {
namespace chronoshare {

/// The class prevent face loss connection to NFD.
class IoServiceManager : private boost::noncopyable
{
public:
  /// Construct the server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.

  IoServiceManager(boost::asio::io_service& io);

  ~IoServiceManager();

  /// Run the service's io_service loop.
  void
  run();

  /// Handle a request to stop the service.
  void
  handle_stop();

private:
  /// the IO service used by NFD connection.
  boost::asio::io_service& m_ioService;
  std::unique_ptr<boost::asio::io_service::work> m_ioServiceWork;
  bool m_connect;
};

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_IO_SERVICE_MANAGER_HPP
