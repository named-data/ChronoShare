#include "ccnx-tunnel.h"

namespace Ccnx
{

CcnxTunnel::CcnxTunnel()
                          : CcnxWrapper()
                          , m_localPrefix("/")
{
  refreshLocalPrefix();
}

CcnxTunnel::~CcnxTunnel() 
{
}

CcnxTunnel::refreshLocalPrefix() 
{
  string newPrefix = getLocalPrefix();
  if (!newPrefix.empty() && m_localPrefix != newPrefix)
  {
    CcnxWrapper::clearInterestFilter(m_localPrefix);
    CcnxWrapper::setInterestFilter(newPrefix, bind(&CcnxTunnel::handleTunneledInterest, this, _1));
    m_localPrefix = newPrefix;
  }
}

int
sendInterest(const string &interest, void *dataPass)
{
  string tunneledInterest = queryRoutableName(interest);
}

int
CccnxTunnel::publishRawData(const string &name, const char *buf, size_t len, int freshness)
{
  ContentObjectPtr co = createContentObject(name, buf, len, freshness);
  storeContentObject(name, co);
  
  string tunneledName = m_localPrefix + name;
  ContentObjectPtr tunneledCo = createContentObject(tunneledName, co->m_data, co->m_len, freshness);

  return putToCcnd(tunneledCo);
}

void
CcnxTunnel::handleTunneledInterest(string tunneledInterest)
{
  // The interest must have m_localPrefix as a prefix (component-wise), otherwise ccnd would not deliver it to us
  string interest = (m_localPrefix == "/") 
                    ? tunneledInterest
                    : tunneldInterest.substr(m_localPrefix.size());

  ReadLock(m_ritLock); 
  
  // This is linear scan, but should be acceptable under the assumption that the caller wouldn't be listening to a lot prefixes (as of now, most app listen to one or two prefixes)
  for (RitIter it = m_rit.begin(); it != m_rit.end(); it++)
  {
    // evoke callback for any prefix that is the prefix of the interest
    if (isPrefix(it->first(), interest))
    {
      (it->second())(interest);
    }
  }
}

bool
CcnxTunnel::isPrefix(string &prefix, string &name)
{
  // prefix is literally prefix of name 
  if (name.find(prefix) == 0)
  {
    // name and prefix are exactly the same, or the next character in name
    // is '/'; in both case, prefix is the ccnx prefix of name (component-wise)
    if (name.size() == prefix.size() || name.at(prefix.size()) == '/')
    {
      return true;
    }
  }
  return false;
}

int 
CcnxTunnel::setInterestFilter(const string &prefix, const InterestCallback &interestCallback)
{
  WriteLock(m_ritLock);
  m_rit.insert(make_pair(prefix, new InterestCallback(interestCallback)));
  return 0;
}

void 
CcnxTunnel::clearInterestFilter(const string &prefix)
{
  WriteLock(m_ritLock);
  // remove all
  m_rit.erase(prefix);
}

}

