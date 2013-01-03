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

#ifndef NOTIFY_I_H
#define NOTIFY_I_H

#include <action-log.h>
#include <chronoshare-client.ice.h>

class NotifyI : public ChronoshareClient::Notify
{
public:
  NotifyI (ActionLogPtr &actionLog);
  
  virtual void
  updateFile (const ::std::string &filename,
              const ::std::pair<const Ice::Byte*, const Ice::Byte*> &hash,
              ::Ice::Long atime,
              ::Ice::Long mtime,
              ::Ice::Long ctime,
              ::Ice::Int mode,
              const ::Ice::Current& = ::Ice::Current());

  virtual void
  moveFile (const ::std::string &oldFilename,
            const ::std::string &newFilename,
            const ::Ice::Current& = ::Ice::Current());
  
  virtual void
  deleteFile (const ::std::string &filename,
              const ::Ice::Current& = ::Ice::Current());

private:
  ActionLogPtr m_actionLog;
};

#endif // NOTIFY_I_H
