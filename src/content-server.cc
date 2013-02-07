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

#include "content-server.h"
#include "logging.h"
#include <boost/make_shared.hpp>
#include <utility>
#include "task.h"
#include "periodic-task.h"
#include "simple-interval-generator.h"
#include <boost/lexical_cast.hpp>

INIT_LOGGER ("ContentServer");

using namespace Ccnx;
using namespace std;
using namespace boost;

static const int DB_CACHE_LIFETIME = 60;

ContentServer::ContentServer(CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
                             const boost::filesystem::path &rootDir,
                             const Ccnx::Name &deviceName, const std::string &sharedFolderName,
                             const std::string &appName,
                             int freshness)
  : m_ccnx(ccnx)
  , m_actionLog(actionLog)
  , m_dbFolder(rootDir / ".chronoshare")
  , m_freshness(freshness)
  , m_scheduler (new Scheduler())
  , m_deviceName (deviceName)
  , m_sharedFolderName (sharedFolderName)
  , m_appName (appName)
{
  m_scheduler->start ();
  TaskPtr flushStaleDbCacheTask = boost::make_shared<PeriodicTask>(boost::bind(&ContentServer::flushStaleDbCache, this), "flush-state-db-cache", m_scheduler, boost::make_shared<SimpleIntervalGenerator>(DB_CACHE_LIFETIME));
  m_scheduler->addTask(flushStaleDbCacheTask);
}

ContentServer::~ContentServer()
{
  m_scheduler->shutdown ();

  ScopedLock lock (m_mutex);
  for (PrefixIt it = m_prefixes.begin(); it != m_prefixes.end(); ++it)
  {
    Name filePrefix   = Name (*it)(m_appName)("file");
    Name actionPrefix = Name (*it)(m_appName)(m_sharedFolderName)("action");

    m_ccnx->clearInterestFilter(filePrefix);
    m_ccnx->clearInterestFilter(actionPrefix);
  }

  m_prefixes.clear ();
}

void
ContentServer::registerPrefix(const Name &forwardingHint)
{
  // Format for files:   /<forwarding-hint>/<appname>/file/<hash>/<device_name>/<segment>
  // Format for actions: /<forwarding-hint>/<appname>/<shared-folder>/action/<device_name>/<action-seq>

  Name filePrefix   = Name (forwardingHint)(m_appName)("file");
  Name actionPrefix = Name (forwardingHint)(m_appName)(m_sharedFolderName)("action");

  m_ccnx->setInterestFilter (filePrefix,   bind(&ContentServer::serve_File,   this, forwardingHint, filePrefix,   _1));
  m_ccnx->setInterestFilter (actionPrefix, bind(&ContentServer::serve_Action, this, forwardingHint, actionPrefix, _1));

  _LOG_DEBUG (">> content server: register FILE   " << filePrefix);
  _LOG_DEBUG (">> content server: register ACTION " << actionPrefix);

  ScopedLock lock (m_mutex);
  m_prefixes.insert(forwardingHint);
}

void
ContentServer::deregisterPrefix (const Name &forwardingHint)
{
  Name filePrefix   = Name (forwardingHint)(m_appName)("file");
  Name actionPrefix = Name (forwardingHint)(m_appName)(m_sharedFolderName)("action");

  m_ccnx->clearInterestFilter(filePrefix);
  m_ccnx->clearInterestFilter(actionPrefix);

  _LOG_DEBUG ("<< content server: deregister FILE   " << filePrefix);
  _LOG_DEBUG ("<< content server: deregister ACTION " << actionPrefix);

  ScopedLock lock (m_mutex);
  m_prefixes.erase (forwardingHint);
}

// void
// ContentServer::serve(Name forwardingHint, const Name &interest)
// {
//   // /forwardingHint/app-name/device-name/action/shared-folder/action-seq
//   // /forwardingHint/app-name/device-name/file/file-hash/segment

//   Name name = interest.getPartialName(forwardingHint.size());
//   if (name.size() > 3)
//   {
//     string type = name.getCompAsString(name.size() - 3);
//     if (type == "action")
//     {
//       serve_Action (forwardingHint, interest);
//     }
//     else if (type == "file")
//     {
//       serve_File (forwardingHint, interest);
//     }
//   }
// }

void
ContentServer::serve_Action (Name forwardingHint, Name locatorPrefix, Name interest)
{
  _LOG_DEBUG (">> content server serving ACTION, hint: " << forwardingHint << ", locatorPrefix: " << locatorPrefix << ", interest: " << interest);
  m_scheduler->scheduleOneTimeTask (m_scheduler, 0, bind (&ContentServer::serve_Action_Execute, this, forwardingHint, locatorPrefix, interest), boost::lexical_cast<string>(interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File (Name forwardingHint, Name locatorPrefix, Name interest)
{
  _LOG_DEBUG (">> content server serving FILE, hint: " << forwardingHint << ", locatorPrefix: " << locatorPrefix << ", interest: " << interest);

  m_scheduler->scheduleOneTimeTask (m_scheduler, 0, bind (&ContentServer::serve_File_Execute, this, forwardingHint, locatorPrefix, interest), boost::lexical_cast<string>(interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File_Execute (Name forwardingHint, Name locatorPrefix, Name interest)
{
  // forwardingHint: /<forwarding-hint>
  // locatorPrefix:  /<forwarding-hint>/<appname>/file
  // interest:       /<forwarding-hint>/<appname>/file/<hash>/<device_name>/<segment>

  Name pureInterest = interest.getPartialName (locatorPrefix.size ());
  // pureInterest:   /<hash>/<device_name>/<segment>

  int64_t segment = pureInterest.getCompFromBackAsInt (0);
  Name deviceName = pureInterest.getPartialName (1, pureInterest.size () - 2);
  Hash hash (head(pureInterest.getComp (0)), pureInterest.getComp (0).size());

  _LOG_DEBUG (" server FILE for device: " << deviceName << ", file_hash: " << hash.shortHash () << " segment: " << segment);

  string hashStr = lexical_cast<string> (hash);

  ObjectDbPtr db;

  ScopedLock(m_dbCacheMutex);
  {
    DbCache::iterator it = m_dbCache.find(hash);
    if (it != m_dbCache.end())
    {
      db = it->second;
    }
    else
    {
      if (ObjectDb::DoesExist (m_dbFolder, deviceName, hashStr)) // this is kind of overkill, as it counts available segments
        {
         db = boost::make_shared<ObjectDb>(m_dbFolder, hashStr);
         m_dbCache.insert(make_pair(hash, db));
        }
      else
        {
          _LOG_ERROR ("ObjectDd doesn't exist for device: " << deviceName << ", file_hash: " << hash.shortHash ());
        }
    }
  }

  if (db)
  {
    BytesPtr co = db->fetchSegment (deviceName, segment);
    if (co)
      {
        if (forwardingHint.size () == 0)
          {
            _LOG_DEBUG (ParsedContentObject (*co).name ());
            m_ccnx->putToCcnd (*co);
          }
        else
          {
            if (m_freshness > 0)
              {
                m_ccnx->publishData(interest, *co, m_freshness);
              }
            else
              {
                m_ccnx->publishData(interest, *co);
              }
          }

      }
    else
      {
        _LOG_ERROR ("ObjectDd exists, but no segment " << segment << " for device: " << deviceName << ", file_hash: " << hash.shortHash ());
      }

  }
}

void
ContentServer::serve_Action_Execute (Name forwardingHint, Name locatorPrefix, Name interest)
{
  // forwardingHint: /<forwarding-hint>
  // locatorPrefix:  /<forwarding-hint>/<appname>/<shared-folder>/action
  // interest:       /<forwarding-hint>/<appname>/<shared-folder>/action/<device_name>/<action-seq>

  Name pureInterest = interest.getPartialName (locatorPrefix.size ());
  // pureInterest:  /<device_name>/<action-seq>

  int64_t seqno = pureInterest.getCompFromBackAsInt (0);
  Name deviceName = pureInterest.getPartialName (0, pureInterest.size () - 1);

  _LOG_DEBUG (" server ACTION for device: " << deviceName << " and seqno: " << seqno);

  PcoPtr pco = m_actionLog->LookupActionPco (deviceName, seqno);
  if (pco)
    {
      if (forwardingHint.size () == 0)
        {
          m_ccnx->putToCcnd (pco->buf ());
        }
      else
        {
          const Bytes &content = pco->buf ();
          if (m_freshness > 0)
            {
              m_ccnx->publishData(interest, content, m_freshness);
            }
          else
            {
              m_ccnx->publishData(interest, content);
            }
        }
    }
  else
    {
      _LOG_ERROR ("ACTION not found for device: " << deviceName << " and seqno: " << seqno);
    }
}

void
ContentServer::flushStaleDbCache()
{
  ScopedLock(m_dbCacheMutex);
  DbCache::iterator it = m_dbCache.begin();
  while (it != m_dbCache.end())
  {
    ObjectDbPtr db = it->second;
    if (db->secondsSinceLastUse() >= DB_CACHE_LIFETIME)
    {
      m_dbCache.erase(it++);
    }
    else
    {
      ++it;
    }
  }
}
