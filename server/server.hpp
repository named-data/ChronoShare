/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
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
//
// server.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "connection.hpp"
#include "connection_manager.hpp"
#include "request_handler.hpp"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <string>

namespace http {
namespace server {

/// The top-level class of the HTTP server.
class server : private boost::noncopyable
{
public:
  /// Construct the server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit server(const std::string& address, const std::string& port, const std::string& doc_root);

  ~server();

  /// Run the server's io_service loop.
  void
  run();

  /// Handle a request to stop the server.
  void
  handle_stop();

private:
  /// Initiate an asynchronous accept operation.
  void
  start_accept();

  /// Handle completion of an asynchronous accept operation.
  void
  handle_accept(const boost::system::error_code& e);

  /// The io_service used to perform asynchronous operations.
  boost::asio::io_service io_service_;

  /// The signal_set is used to register for process termination notifications.
  // boost::asio::signal_set signals_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  /// The connection manager which owns all live connections.
  connection_manager connection_manager_;

  /// The next connection to be accepted.
  connection_ptr new_connection_;

  /// The handler for all incoming requests.
  request_handler request_handler_;
};

} // namespace server
} // namespace http

#endif // HTTP_SERVER_HPP
