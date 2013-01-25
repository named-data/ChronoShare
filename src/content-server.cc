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
#include <boost/lexical_cast.hpp>

INIT_LOGGER ("ContentServer");

using namespace Ccnx;
using namespace std;
using namespace boost;

ContentServer::ContentServer(CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
                             const boost::filesystem::path &rootDir,
                             const Ccnx::Name &deviceName, const std::string &sharedFolderName,
                             int freshness)
  : m_ccnx(ccnx)
  , m_actionLog(actionLog)
  , m_dbFolder(rootDir / ".chronoshare")
  , m_freshness(freshness)
  , m_executor (1)
  , m_deviceName (deviceName)
  , m_sharedFolderName (sharedFolderName)
{
  m_executor.start ();
}

ContentServer::~ContentServer()
{
  m_executor.shutdown ();

  ScopedLock lock (m_mutex);
  for (PrefixIt it = m_prefixes.begin(); it != m_prefixes.end(); ++it)
  {
    m_ccnx->clearInterestFilter (Name (*it)(m_deviceName)("action")(m_sharedFolderName));
    m_ccnx->clearInterestFilter (Name (*it)(m_deviceName)("file"));
  }
}

void
ContentServer::registerPrefix(const Name &prefix)
{
  Name actionPrefix = Name (prefix)(m_deviceName)("action")(m_sharedFolderName);
  Name filePrefix = Name (prefix)(m_deviceName)("file");
  m_ccnx->setInterestFilter (actionPrefix, bind(&ContentServer::serve_Action, this, prefix, _1));
  m_ccnx->setInterestFilter (filePrefix, bind(&ContentServer::serve_File,   this, prefix, _1));

  _LOG_DEBUG (">> content server: register " << actionPrefix);
  _LOG_DEBUG (">> content server: register " << filePrefix);

  ScopedLock lock (m_mutex);
  m_prefixes.insert(prefix);
}

void
ContentServer::deregisterPrefix (const Name &prefix)
{
  Name actionPrefix = Name (prefix)(m_deviceName)("action")(m_sharedFolderName);
  Name filePrefix = Name (prefix)(m_deviceName)("file");

  m_ccnx->clearInterestFilter(actionPrefix);
  m_ccnx->clearInterestFilter(filePrefix);

  _LOG_DEBUG ("<< content server: deregister " << actionPrefix);
  _LOG_DEBUG ("<< content server: deregister " << filePrefix);
  ScopedLock lock (m_mutex);
  m_prefixes.erase (prefix);
}

void
ContentServer::serve_Action (Name forwardingHint, const Name &interest)
{
  _LOG_DEBUG (">> content server serving, forwardHing " << forwardingHint << ", interest " << interest);
  m_executor.execute (bind (&ContentServer::serve_Action_Execute, this, forwardingHint, interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File (Name forwardingHint, const Name &interest)
{
  _LOG_DEBUG (">> content server serving, forwardHing " << forwardingHint << ", interest " << interest);
  m_executor.execute (bind (&ContentServer::serve_File_Execute, this, forwardingHint, interest));
  // need to unlock ccnx mutex... or at least don't lock it
}

void
ContentServer::serve_File_Execute (Name forwardingHint, Name interest)
{
  // /device-name/file/<file-hash>/segment, or

  int64_t segment = interest.getCompFromBackAsInt (0);
  Name deviceName = interest.getPartialName (forwardingHint.size (), interest.size () - forwardingHint.size () - 3);
  Hash hash (head(interest.getCompFromBack (1)), interest.getCompFromBack (1).size());

  _LOG_DEBUG (" server FILE for device: " << deviceName << ", file_hash: " << hash << " segment: " << segment);

  string hashStr = lexical_cast<string> (hash);
  if (ObjectDb::DoesExist (m_dbFolder, deviceName, hashStr)) // this is kind of overkill, as it counts available segments
    {
      ObjectDb db (m_dbFolder, hashStr);
      // may do prefetching

      BytesPtr co = db.fetchSegment (deviceName, segment);
      if (co)
        {
          if (forwardingHint.size () == 0)
            {
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
          _LOG_ERROR ("ObjectDd exists, but no segment " << segment << " for device: " << deviceName << ", file_hash: " << hash);
        }
    }
  else
    {
      _LOG_ERROR ("ObjectDd doesn't exist for device: " << deviceName << ", file_hash: " << hash);
    }

}

void
ContentServer::serve_Action_Execute (Name forwardingHint, Name interest)
{
  // /device-name/action/shared-folder/seq

  int64_t seqno = interest.getCompFromBackAsInt (0);
  Name deviceName = interest.getPartialName (forwardingHint.size (), interest.size () - forwardingHint.size () - 3);

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
