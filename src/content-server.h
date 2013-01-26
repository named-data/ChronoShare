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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef CONTENT_SERVER_H
#define CONTENT_SERVER_H

#include "ccnx-wrapper.h"
#include "object-db.h"
#include "action-log.h"
#include <set>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include "executor.h"

class ContentServer
{
public:
  ContentServer(Ccnx::CcnxWrapperPtr ccnx, ActionLogPtr actionLog, const boost::filesystem::path &rootDir,
                const Ccnx::Name &deviceName, const std::string &sharedFolderName, int freshness = -1);
  ~ContentServer();

  // the assumption is, when the interest comes in, interest is informs of
  // /some-prefix/topology-independent-name
  // currently /topology-independent-name must begin with /action or /file
  // so that ContentServer knows where to look for the content object
  void registerPrefix(const Ccnx::Name &prefix);
  void deregisterPrefix(const Ccnx::Name &prefix);

private:
  void
  serve (Ccnx::Name forwardingHint, const Ccnx::Name &interest);

  void
  serve_Action (Ccnx::Name forwardingHint, const Ccnx::Name &interest);

  void
  serve_File (Ccnx::Name forwardingHint, const Ccnx::Name &interest);

  void
  serve_Action_Execute(Ccnx::Name forwardingHint, Ccnx::Name interest);

  void
  serve_File_Execute(Ccnx::Name forwardingHint, Ccnx::Name interest);

private:
  Ccnx::CcnxWrapperPtr m_ccnx;
  ActionLogPtr m_actionLog;
  typedef boost::shared_mutex Mutex;

  typedef boost::unique_lock<Mutex> ScopedLock;
  typedef std::set<Ccnx::Name>::iterator PrefixIt;
  std::set<Ccnx::Name> m_prefixes;
  Mutex m_mutex;
  boost::filesystem::path m_dbFolder;
  int m_freshness;

  Executor     m_executor;

  Ccnx::Name  m_deviceName;
  std::string m_sharedFolderName;
};
#endif // CONTENT_SERVER_H
