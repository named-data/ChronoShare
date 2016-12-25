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

#ifndef CONTENT_SERVER_H
#define CONTENT_SERVER_H

#include "core/chronoshare-common.hpp"

#include "action-log.hpp"
#include "object-db.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/scheduler.hpp>

#include <map>
#include <set>

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ndn {
namespace chronoshare {

class ContentServer
{
public:
  ContentServer(Face& face, ActionLogPtr actionLog, const boost::filesystem::path& rootDir,
                const Name& userName, const std::string& sharedFolderName,
                const name::Component& appName, KeyChain& keyChain,
                time::milliseconds freshness = time::seconds(5));
  ~ContentServer();

  // the assumption is, when the interest comes in, interest is informs of
  // /some-prefix/topology-independent-name
  // currently /topology-independent-name must begin with /action or /file
  // so that ContentServer knows where to look for the content object
  void
  registerPrefix(const Name& prefix);
  void
  deregisterPrefix(const Name& prefix);

private:
  void
  filterAndServe(const InterestFilter& forwardingHint, const Interest& interest);

  void
  filterAndServeImpl(const Name& forwardingHint, const Name& name, const Name& interest);

  void
  serve_Action(const Name& forwardingHint, const Name& name, const Name& interest);

  void
  serve_File(const Name& forwardingHint, const Name& name, const Name& interest);

  void
  serve_Action_Execute(const Name& forwardingHint, const Name& name, const Name& interest);

  void
  serve_File_Execute(const Name& forwardingHint, const Name& name, const Name& interest);

  void
  flushStaleDbCache();

private:
  Face& m_face;
  ActionLogPtr m_actionLog;
  typedef boost::shared_mutex Mutex;
  typedef boost::unique_lock<Mutex> ScopedLock;

  typedef std::map<Name, const RegisteredPrefixId*>::iterator FilterIdIt;
  std::map<Name, const RegisteredPrefixId*> m_interestFilterIds;

  Mutex m_mutex;
  boost::filesystem::path m_dbFolder;
  time::milliseconds m_freshness;

  Scheduler m_scheduler;
  util::scheduler::ScopedEventId m_flushStateDbCacheEvent;
  typedef std::map<Buffer, shared_ptr<ObjectDb>> DbCache;
  DbCache m_dbCache;
  Mutex m_dbCacheMutex;

  Name m_userName;
  std::string m_sharedFolderName;
  name::Component m_appName;
  KeyChain& m_keyChain;
};

} // namespace chronoshare
} // namespace ndn

#endif // CONTENT_SERVER_H
