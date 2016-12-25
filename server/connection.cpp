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
// connection.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "connection.hpp"
#include "connection_manager.hpp"
#include "request_handler.hpp"
#include <boost/bind.hpp>
#include <vector>

namespace http {
namespace server {

connection::connection(boost::asio::io_service& io_service, connection_manager& manager,
                       request_handler& handler)
  : socket_(io_service)
  , connection_manager_(manager)
  , request_handler_(handler)
{
}

boost::asio::ip::tcp::socket&
connection::socket()
{
  return socket_;
}

void
connection::start()
{
  socket_.async_read_some(boost::asio::buffer(buffer_),
                          boost::bind(&connection::handle_read, shared_from_this(),
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::bytes_transferred));
}

void
connection::stop()
{
  socket_.close();
}

void
connection::handle_read(const boost::system::error_code& e, std::size_t bytes_transferred)
{
  if (!e) {
    boost::tribool result;
    boost::tie(result, boost::tuples::ignore) =
      request_parser_.parse(request_, buffer_.data(), buffer_.data() + bytes_transferred);

    if (result) {
      request_handler_.handle_request(request_, reply_);
      boost::asio::async_write(socket_, reply_.to_buffers(),
                               boost::bind(&connection::handle_write, shared_from_this(),
                                           boost::asio::placeholders::error));
    }
    else if (!result) {
      reply_ = reply::stock_reply(reply::bad_request);
      boost::asio::async_write(socket_, reply_.to_buffers(),
                               boost::bind(&connection::handle_write, shared_from_this(),
                                           boost::asio::placeholders::error));
    }
    else {
      socket_.async_read_some(boost::asio::buffer(buffer_),
                              boost::bind(&connection::handle_read, shared_from_this(),
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    }
  }
  else if (e != boost::asio::error::operation_aborted) {
    connection_manager_.stop(shared_from_this());
  }
}

void
connection::handle_write(const boost::system::error_code& e)
{
  if (!e) {
    // Initiate graceful connection closure.
    boost::system::error_code ignored_ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
  }

  if (e != boost::asio::error::operation_aborted) {
    connection_manager_.stop(shared_from_this());
  }
}

} // namespace server
} // namespace http
