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
using namespace boost;

NotifyI::NotifyI (ActionLogPtr &actionLog)
  : m_actionLog (actionLog)
{
}

void
NotifyI::updateFile (const ::std::string &filename,
                     const ::std::pair<const Ice::Byte*, const Ice::Byte*> &hashRaw,
                     ::Ice::Long wtime,
                     ::Ice::Int mode,
                     const ::Ice::Current&)
{
  Hash hash (hashRaw.first, hashRaw.second-hashRaw.first);

  cout << "updateFile " << filename << " with hash " << hash << endl;
  try
    {
      m_actionLog->AddActionUpdate (filename, hash, wtime, mode);

      m_actionLog->RememberStateInStateLog ();
    }
  catch (const boost::exception &e)
    {
      cout << "ERRORR: " << *get_error_info<errmsg_info_str> (e) << endl;
    }
}

void
NotifyI::moveFile (const ::std::string &oldFilename,
                   const ::std::string &newFilename,
                   const ::Ice::Current&)
{
  cout << "moveFile from " << oldFilename << " to " << newFilename << endl;
  // m_actionLog->AddActionMove (oldFilename, newFilename);
}

void
NotifyI::deleteFile (const ::std::string &filename,
                     const ::Ice::Current&)
{
  cout << "deleteFile " << filename << endl;
  m_actionLog->AddActionDelete (filename);  
  m_actionLog->RememberStateInStateLog ();
}

