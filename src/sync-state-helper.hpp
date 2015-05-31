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

#ifndef CHRONOSHARE_SRC_SYNC_STATE_HELPER_HPP
#define CHRONOSHARE_SRC_SYNC_STATE_HELPER_HPP

#include "core/chronoshare-common.hpp"

#include "sync-state.pb.h"

namespace ndn {
namespace chronoshare {

inline std::ostream&
operator<<(std::ostream& os, const SyncStateMsgPtr& msg)
{
  os << " ===== start Msg ======" << std::endl;

  int size = msg->state_size();
  if (size > 0) {
    int index = 0;
    while (index < size) {
      SyncState state = msg->state(index);
      std::string strName = state.name();
      std::string strLocator = state.locator();
      sqlite3_int64 seq = state.seq();

      os << "Name: " << Name(Block((const unsigned char*)strName.c_str(), strName.size())).toUri()
         << ", Locator: " << Name(Block((const unsigned char*)strLocator.c_str(), strLocator.size()))
         << ", seq: " << seq << std::endl;
      index++;
    }
  }
  else {
    os << "Msg size 0" << std::endl;
  }
  os << " ++++++++ end Msg  ++++++++ " << std::endl;

  return os;
}

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_SYNC_STATE_HELPER_HPP
