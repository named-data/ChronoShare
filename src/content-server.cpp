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

#include "content-server.hpp"
#include "core/logging.hpp"

#include <ndn-cxx/util/string-helper.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>

#include <boost/lexical_cast.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(ContentServer);

static const int DB_CACHE_LIFETIME = 60;

ContentServer::ContentServer(Face& face, ActionLogPtr actionLog,
                             const boost::filesystem::path& rootDir, const Name& userName,
                             const std::string& sharedFolderName, const name::Component& appName,
                             KeyChain& keyChain, time::milliseconds freshness)
  : m_face(face)
  , m_actionLog(actionLog)
  , m_dbFolder(rootDir / ".chronoshare")
  , m_freshness(freshness)
  , m_scheduler(face.getIoService())
  , m_flushStateDbCacheEvent(m_scheduler)
  , m_userName(userName)
  , m_sharedFolderName(sharedFolderName)
  , m_appName(appName)
  , m_keyChain(keyChain)
{
  m_flushStateDbCacheEvent = m_scheduler.scheduleEvent(time::seconds(DB_CACHE_LIFETIME),
                                                       bind(&ContentServer::flushStaleDbCache, this));
}

ContentServer::~ContentServer()
{
  ScopedLock lock(m_mutex);
  for (FilterIdIt it = m_interestFilterIds.begin(); it != m_interestFilterIds.end(); ++it) {
    m_face.unsetInterestFilter(it->second);
  }

  m_interestFilterIds.clear();
}

void
ContentServer::registerPrefix(const Name& forwardingHint)
{
  // Format for files:   /<forwarding-hint>/<device_name>/<appname>/file/<hash>/<segment>
  // Format for actions:
  // /<forwarding-hint>/<device_name>/<appname>/action/<shared-folder>/<action-seq>

  _LOG_DEBUG(">> content server: register " << forwardingHint);

  ScopedLock lock(m_mutex);
  m_interestFilterIds[forwardingHint] =
    m_face.setInterestFilter(InterestFilter(forwardingHint),
                             bind(&ContentServer::filterAndServe, this, _1, _2),
                             RegisterPrefixSuccessCallback(), RegisterPrefixFailureCallback());
}

void
ContentServer::deregisterPrefix(const Name& forwardingHint)
{
  _LOG_DEBUG("<< content server: deregister " << forwardingHint);
  m_face.unsetInterestFilter(m_interestFilterIds[forwardingHint]);

  ScopedLock lock(m_mutex);
  m_interestFilterIds.erase(forwardingHint);
}

void
ContentServer::filterAndServeImpl(const Name& forwardingHint, const Name& name, const Name& interest)
{
  // interest for files:   /<forwarding-hint>/<device_name>/<appname>/file/<hash>/<segment>
  // interest for actions:
  // /<forwarding-hint>/<device_name>/<appname>/action/<shared-folder>/<action-seq>

  // name for files:   /<device_name>/<appname>/file/<hash>/<segment>
  // name for actions: /<device_name>/<appname>/action/<shared-folder>/<action-seq>

  if (name.size() >= 4 && name.get(-4) == m_appName) {
    std::string type = name.get(-3).toUri();
    if (type == "file") {
      serve_File(forwardingHint, name, interest);
    }
    else if (type == "action") {
      std::string folder = name.get(-2).toUri();
      if (folder == m_sharedFolderName) {
        serve_Action(forwardingHint, name, interest);
      }
    }
  }
}

void
ContentServer::filterAndServe(const InterestFilter& interestFilter, const Interest& interestTrue)
{
  Name forwardingHint = Name(interestFilter);
  Name interest = interestTrue.getName();
  _LOG_TRACE("Serving ForwardingHint: " << forwardingHint << " Interest: " << interest);
  if (forwardingHint.size() > 0 && m_userName.size() >= forwardingHint.size() &&
      m_userName.getSubName(0, forwardingHint.size()) == forwardingHint) {
    _LOG_DEBUG("Triggered without Forwardinghint!");
    filterAndServeImpl(Name("/"), interest, interest); // try without forwarding hints
  }

  _LOG_TRACE("Triggered with Forwardinghint~!");
  filterAndServeImpl(forwardingHint, interest.getSubName(forwardingHint.size()),
                     interest); // always try with hint... :( have to
}

void
ContentServer::serve_Action(const Name& forwardingHint, const Name& name, const Name& interest)
{
  _LOG_DEBUG(
    ">> content server serving ACTION, hint: " << forwardingHint << ", interest: " << interest);
  m_scheduler.scheduleEvent(time::seconds(0),
                            bind(&ContentServer::serve_Action_Execute, this, forwardingHint, name,
                                 interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File(const Name& forwardingHint, const Name& name, const Name& interest)
{
  _LOG_DEBUG(">> content server serving FILE, hint: " << forwardingHint << ", interest: " << interest);

  m_scheduler.scheduleEvent(time::seconds(0),
                            bind(&ContentServer::serve_File_Execute, this, forwardingHint, name,
                                 interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File_Execute(const Name& forwardingHint, const Name& name, const Name& interest)
{
  // forwardingHint: /<forwarding-hint>
  // interest:       /<forwarding-hint>/<device_name>/<appname>/file/<hash>/<segment>
  // name:           /<device_name>/<appname>/file/<hash>/<segment>

  int64_t segment = name.get(-1).toNumber();
  Name deviceName = name.getSubName(0, name.size() - 4);
  Buffer hash(name.get(-2).value(), name.get(-2).value_size());

  _LOG_DEBUG(" server FILE for device: " << deviceName << ", file_hash: " << toHex(hash) << " segment: "
                                         << segment);

  std::string hashStr = toHex(hash);

  shared_ptr<ObjectDb> db;

  ScopedLock(m_dbCacheMutex);
  {
    DbCache::iterator it = m_dbCache.find(hash);
    if (it != m_dbCache.end()) {
      db = it->second;
    }
    else {
      if (ObjectDb::doesExist(m_dbFolder, deviceName,
                              hashStr)) // this is kind of overkill, as it counts available segments
      {
        db = make_shared<ObjectDb>(m_dbFolder, hashStr);
        m_dbCache.insert(make_pair(hash, db));
      }
      else {
        _LOG_ERROR(
          "ObjectDd doesn't exist for device: " << deviceName << ", file_hash: " << toHex(hash));
      }
    }
  }

  if (db) {
    shared_ptr<Data> data = db->fetchSegment(deviceName, segment);
    if (data) {
      if (forwardingHint.size() == 0) {
        m_face.put(*data);
      }
      else {
        shared_ptr<Data> outerData = make_shared<Data>();

        outerData->setContent(data->wireEncode());
        outerData->setFreshnessPeriod(m_freshness);
        outerData->setName(interest);

        m_keyChain.sign(*outerData, signingWithSha256());;
        m_face.put(*outerData);
      }
      _LOG_DEBUG("Send File Data Done!");
    }
    else {
      _LOG_ERROR("ObjectDd exists, but no segment " << segment << " for device: " << deviceName
                                                    << ", file_hash: "
                                                    << toHex(hash));
    }
  }
}

void
ContentServer::serve_Action_Execute(const Name& forwardingHint, const Name& name, const Name& interest)
{
  // forwardingHint: /<forwarding-hint>
  // interest:       /<forwarding-hint>/<device_name>/<appname>/action/<shared-folder>/<action-seq>
  // name for actions: /<device_name>/<appname>/action/<shared-folder>/<action-seq>

  int64_t seqno = name.get(-1).toNumber();
  Name deviceName = name.getSubName(0, name.size() - 4);

  _LOG_DEBUG(" server ACTION for device: " << deviceName << " and seqno: " << seqno);

  shared_ptr<Data> data = m_actionLog->LookupActionData(deviceName, seqno);
  if (data) {
    if (forwardingHint.size() == 0) {
      m_keyChain.sign(*data);
      m_face.put(*data);
    }
    else {
      data->setName(interest);
      data->setFreshnessPeriod(m_freshness);
      m_keyChain.sign(*data);
      m_face.put(*data);
    }
  }
  else {
    _LOG_ERROR("ACTION not found for device: " << deviceName << " and seqno: " << seqno);
  }
}

void
ContentServer::flushStaleDbCache()
{
  ScopedLock(m_dbCacheMutex);
  DbCache::iterator it = m_dbCache.begin();
  while (it != m_dbCache.end()) {
    shared_ptr<ObjectDb> db = it->second;
    if (time::steady_clock::now() >= time::seconds(DB_CACHE_LIFETIME) + db->getLastUsed()) {
      m_dbCache.erase(it++);
    }
    else {
      ++it;
    }
  }

  m_flushStateDbCacheEvent = m_scheduler.scheduleEvent(time::seconds(DB_CACHE_LIFETIME),
                                                       bind(&ContentServer::flushStaleDbCache, this));
}

} // namespace chronoshare
} // namespace ndn
