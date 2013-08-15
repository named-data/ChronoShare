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

#include "ndnx-tunnel.h"
#include "ndnx-pco.h"

namespace Ndnx
{

NdnxTunnel::NdnxTunnel()
  : NdnxWrapper()
  , m_localPrefix("/")
{
  refreshLocalPrefix();
}

NdnxTunnel::~NdnxTunnel()
{
}

void
NdnxTunnel::refreshLocalPrefix()
{
  Name newPrefix = getLocalPrefix();
  if (!newPrefix.toString().empty() && m_localPrefix != newPrefix)
  {
    NdnxWrapper::clearInterestFilter(m_localPrefix);
    NdnxWrapper::setInterestFilter(newPrefix, bind(&NdnxTunnel::handleTunneledInterest, this, _1));
    m_localPrefix = newPrefix;
  }
}

int
NdnxTunnel::sendInterest (const Name &interest, const Closure &closure, const Selectors &selectors)
{
  Name tunneledInterest = queryRoutableName(interest);

  NdnxWrapper::sendInterest(tunneledInterest,
                            TunnelClosure(closure, *this, interest),
                            selectors);

  return 0;
}

void
NdnxTunnel::handleTunneledData(const Name &name, const Bytes &tunneledData, const Closure::DataCallback &originalDataCallback)
{
  ParsedContentObject pco(tunneledData);
  originalDataCallback(pco.name(), pco.content());
}

int
NdnxTunnel::publishData(const Name &name, const unsigned char *buf, size_t len, int freshness)
{
  Bytes content = createContentObject(name, buf, len, freshness);
  storeContentObject(name, content);

  return publishContentObject(name, content, freshness);
}

int
NdnxTunnel::publishContentObject(const Name &name, const Bytes &contentObject, int freshness)
{
  Name tunneledName = m_localPrefix + name;
  Bytes tunneledCo = createContentObject(tunneledName, head(contentObject), contentObject.size(), freshness);
  return putToNdnd(tunneledCo);
}

void
NdnxTunnel::handleTunneledInterest(const Name &tunneledInterest)
{
  // The interest must have m_localPrefix as a prefix (component-wise), otherwise ndnd would not deliver it to us
  Name interest = tunneledInterest.getPartialName(m_localPrefix.size());

  ReadLock lock(m_ritLock);

  // This is linear scan, but should be acceptable under the assumption that the caller wouldn't be listening to a lot prefixes (as of now, most app listen to one or two prefixes)
  for (RitIter it = m_rit.begin(); it != m_rit.end(); it++)
  {
    // evoke callback for any prefix that is the prefix of the interest
    if (isPrefix(it->first, interest))
    {
      (it->second)(interest);
    }
  }
}

bool
NdnxTunnel::isPrefix(const Name &prefix, const Name &name)
{
  if (prefix.size() > name.size())
  {
    return false;
  }

  int size = prefix.size();
  for (int i = 0; i < size; i++)
  {
    if (prefix.getCompAsString(i) != name.getCompAsString(i))
    {
      return false;
    }
  }

  return true;
}

int
NdnxTunnel::setInterestFilter(const Name &prefix, const InterestCallback &interestCallback)
{
  WriteLock lock(m_ritLock);
  // make sure copy constructor for boost::function works properly
  m_rit.insert(make_pair(prefix, interestCallback));
  return 0;
}

void
NdnxTunnel::clearInterestFilter(const Name &prefix)
{
  WriteLock lock(m_ritLock);
  // remove all
  m_rit.erase(prefix);
}

TunnelClosure::TunnelClosure(const DataCallback &dataCallback, NdnxTunnel &tunnel,
                             const Name &originalInterest, const TimeoutCallback &timeoutCallback)
  : Closure(dataCallback, timeoutCallback)
  , m_tunnel(tunnel)
  , m_originalInterest(originalInterest)
{
}

TunnelClosure::TunnelClosure(const Closure &closure, NdnxTunnel &tunnel, const Name &originalInterest)
  : Closure(closure)
  , m_tunnel(tunnel)
{
}

Closure *
TunnelClosure::dup() const
{
  return new TunnelClosure (*this);
}

void
TunnelClosure::runDataCallback(const Name &name, const Bytes &content)
{
  m_tunnel.handleTunneledData(name, content, m_dataCallback);
}

Closure::TimeoutCallbackReturnValue
TunnelClosure::runTimeoutCallback(const Name &interest)
{
  return Closure::runTimeoutCallback(m_originalInterest);
}

} // Ndnx
