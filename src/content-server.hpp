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

#ifndef CONTENT_SERVER_H
#define CONTENT_SERVER_H

#include "action-log.hpp"
#include "ccnx-wrapper.hpp"
#include "object-db.hpp"
#include "scheduler.hpp"
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <set>

class ContentServer
{
public:
  ContentServer(Ccnx::CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
                const boost::filesystem::path& rootDir, const Ccnx::Name& userName,
                const std::string& sharedFolderName, const std::string& appName, int freshness = -1);
  ~ContentServer();

  // the assumption is, when the interest comes in, interest is informs of
  // /some-prefix/topology-independent-name
  // currently /topology-independent-name must begin with /action or /file
  // so that ContentServer knows where to look for the content object
  void
  registerPrefix(const Ccnx::Name& prefix);
  void
  deregisterPrefix(const Ccnx::Name& prefix);

private:
  void
  filterAndServe(Ccnx::Name forwardingHint, const Ccnx::Name& interest);

  void
  filterAndServeImpl(const Ccnx::Name& forwardingHint, const Ccnx::Name& name,
                     const Ccnx::Name& interest);

  void
  serve_Action(const Ccnx::Name& forwardingHint, const Ccnx::Name& name, const Ccnx::Name& interest);

  void
  serve_File(const Ccnx::Name& forwardingHint, const Ccnx::Name& name, const Ccnx::Name& interest);

  void
  serve_Action_Execute(const Ccnx::Name& forwardingHint, const Ccnx::Name& name,
                       const Ccnx::Name& interest);

  void
  serve_File_Execute(const Ccnx::Name& forwardingHint, const Ccnx::Name& name,
                     const Ccnx::Name& interest);

  void
  flushStaleDbCache();

private:
  Ndnx::NdnxWrapperPtr m_ndnx;
  ActionLogPtr m_actionLog;
  typedef boost::shared_mutex Mutex;

  typedef boost::unique_lock<Mutex> ScopedLock;
  typedef std::set<Ndnx::Name>::iterator PrefixIt;
  std::set<Ndnx::Name> m_prefixes;
  Mutex m_mutex;
  boost::filesystem::path m_dbFolder;
  int m_freshness;

  SchedulerPtr m_scheduler;
  typedef std::map<Hash, ObjectDbPtr> DbCache;
  DbCache m_dbCache;
  Mutex m_dbCacheMutex;

  Ccnx::Name m_userName;
  std::string m_sharedFolderName;
  std::string m_appName;
};
#endif // CONTENT_SERVER_H
