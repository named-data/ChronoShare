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

#include "notify-i.h"
#include <hash-helper.h>

using namespace std;

// NotifyI::NotifyI (DbHelperPtr &db)
//   : m_db (db)
// {
// }

void
NotifyI::updateFile (const ::std::string &filename,
                     const ::std::pair<const Ice::Byte*, const Ice::Byte*> &hashRaw,
                     const ::std::string &atime,
                     const ::std::string &mtime,
                     const ::std::string &ctime,
                     ::Ice::Int mode,
                     const ::Ice::Current&)
{
  Hash hash (hashRaw.first, hashRaw.second-hashRaw.first);

  // m_db->AddActionUpdate (filename, hash, atime, mtime, ctime, mode);
  
  // cout << "updateFile " << filename << " with hash " << hash << endl;
}

void
NotifyI::moveFile (const ::std::string &oldFilename,
                   const ::std::string &newFilename,
                   const ::Ice::Current&)
{
  // cout << "moveFile from " << oldFilename << " to " << newFilename << endl;
  // m_db->AddActionMove (filename, oldFilename);
}

void
NotifyI::deleteFile (const ::std::string &filename,
                     const ::Ice::Current&)
{
  // m_db->AddActionDelete (filename, oldFilename);
  
  // cout << "deleteFile " << filename << endl;
}

