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
// mime_types.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "mime_types.hpp"

namespace http {
namespace server {
namespace mime_types {

struct mapping
{
  const char* extension;
  const char* mime_type;
} mappings[] = {
  {"gif", "image/gif"}, {"htm", "text/html"},      {"html", "text/html"},
  {"css", "text/css"},  {"js", "text/javascript"}, {"jpg", "image/jpeg"},
  {"png", "image/png"}, {"ico", "image/x-icon"},   {0, 0} // Marks end of list.
};

std::string
extension_to_type(const std::string& extension)
{
  for (mapping* m = mappings; m->extension; ++m) {
    if (m->extension == extension) {
      return m->mime_type;
    }
  }

  return "text/plain";
}

} // namespace mime_types
} // namespace server
} // namespace http
