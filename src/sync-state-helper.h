/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef SYNC_STATE_HELPER_H
#define SYNC_STATE_HELPER_H

#include "sync-state.pb.h"

inline std::ostream &
operator << (std::ostream &os, const SyncStateMsgPtr &msg)
{
  os << " ===== start Msg ======" << std::endl;

  int size = msg->state_size();
  if (size > 0)
  {
    int index = 0;
    while (index < size)
    {
      SyncState state = msg->state(index);
      string strName = state.name();
      string strLocator = state.locator();
      sqlite3_int64 seq = state.seq();

      os << "Name: " << Ccnx::Name((const unsigned char *)strName.c_str(), strName.size())
         << ", Locator: " << Ccnx::Name((const unsigned char *)strLocator.c_str(), strLocator.size())
         << ", seq: " << seq << std::endl;
      index ++;
    }
  }
  else
  {
    os << "Msg size 0" << std::endl;
  }
  os << " ++++++++ end Msg  ++++++++ " << std::endl;

  return os;
}


#endif // SYNC_STATE_HELPER_H
