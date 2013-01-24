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

using namespace Ccnx;
using namespace std;

ContentServer::ContentServer(CcnxWrapperPtr ccnx, ActionLogPtr actionLog, const boost::filesystem::path &rootDir, int freshness)
              : m_ccnx(ccnx)
              , m_actionLog(actionLog)
              , m_dbFolder(rootDir / ".chronoshare")
              , m_freshness(freshness)
{
}

ContentServer::~ContentServer()
{
  WriteLock lock(m_mutex);
  int size = m_prefixes.size();
  for (PrefixIt it = m_prefixes.begin(); it != m_prefixes.end(); ++it)
  {
    m_ccnx->clearInterestFilter(*it);
  }
}

void
ContentServer::registerPrefix(const Name &prefix)
{
  m_ccnx->setInterestFilter(prefix, bind(&ContentServer::serve, this, _1));
  WriteLock lock(m_mutex);
  m_prefixes.insert(prefix);
}

void
ContentServer::deregisterPrefix(const Name &prefix)
{
  WriteLock lock(m_mutex);
  PrefixIt it = m_prefixes.find(prefix);
  if (it != m_prefixes.end())
  {
    m_ccnx->clearInterestFilter(*it);
    m_prefixes.erase(it);
  }
}

void
ContentServer::serve(const Name &interest)
{
  ReadLock lock(m_mutex);
  for (PrefixIt it = m_prefixes.begin(); it != m_prefixes.end(); ++it)
  {
    Name prefix = *it;
    int prefixSize = prefix.size();
    int interestSize = interest.size();
    // this is the prefix of the interest
    if (prefixSize <= interestSize && interest.getPartialName(0, prefixSize) == prefix)
    {
      // originalName should be either
      // /device-name/file/file-hash/segment, or
      // /device-name/action/shared-folder/seq
      Name originalName = interest.getPartialName(prefixSize);
      int nameSize = originalName.size();
      if (nameSize > 3)
      {
        Name deviceName = originalName.getPartialName(0, nameSize - 3);
        string type = originalName.getCompAsString(nameSize - 3);
        BytesPtr co;
        if (type == "action")
        {
          // TODO:
          // get co from m_actionLog
        }
        else if (type == "file")
        {
          Bytes hashBytes = originalName.getComp(nameSize - 2);
          Hash hash(head(hashBytes), hashBytes.size());
          ostringstream oss;
          oss << hash;
          int64_t segment = originalName.getCompAsInt(nameSize - 1);
          ObjectDb db(m_dbFolder, oss.str());
          co = db.fetchSegment(deviceName, segment);
        }
        else
        {
          cerr << "Discard interest that ContentServer does not know how to handle: " << interest << ", prefix: " << prefix << endl;
        }

        if (co)
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
    }
  }
}
