#include "ccnx-tunnel.h"
#include "ccnx-pco.h"

namespace Ccnx
{

CcnxTunnel::CcnxTunnel()
                          : CcnxWrapper()
                          , m_localPrefix("/")
{
  //refreshLocalPrefix();
}

CcnxTunnel::~CcnxTunnel()
{
}

void
CcnxTunnel::refreshLocalPrefix()
{
  Name newPrefix = getLocalPrefix();
  if (!newPrefix.toString().empty() && m_localPrefix != newPrefix)
  {
    CcnxWrapper::clearInterestFilter(m_localPrefix);
    CcnxWrapper::setInterestFilter(newPrefix, bind(&CcnxTunnel::handleTunneledInterest, this, _1));
    m_localPrefix = newPrefix;
  }
}

int
CcnxTunnel::sendInterest (const Name &interest, Closure *closure, const Selectors &selectors)
{
  Name tunneledInterest = queryRoutableName(interest);
  Closure *cp = new TunnelClosure(closure, this, interest);
  CcnxWrapper::sendInterest(tunneledInterest, cp, selectors);
}

void
CcnxTunnel::handleTunneledData(const Name &name, const Bytes &tunneledData, const Closure::DataCallback &originalDataCallback)
{
  ParsedContentObject pco(tunneledData);
  originalDataCallback(pco.name(), pco.content());
}

int
CcnxTunnel::publishData(const Name &name, const unsigned char *buf, size_t len, int freshness)
{
  Bytes content = createContentObject(name, buf, len, freshness);
  storeContentObject(name, content);

  return publishContentObject(name, content, freshness);
}

int
CcnxTunnel::publishContentObject(const Name &name, const Bytes &contentObject, int freshness)
{
  Name tunneledName = m_localPrefix + name;
  cout << "Outer name " << tunneledName;
  Bytes tunneledCo = createContentObject(tunneledName, head(contentObject), contentObject.size(), freshness);
  return putToCcnd(tunneledCo);
}

void
CcnxTunnel::handleTunneledInterest(const Name &tunneledInterest)
{
  // The interest must have m_localPrefix as a prefix (component-wise), otherwise ccnd would not deliver it to us
  Name interest = tunneledInterest.getPartialName(m_localPrefix.size());

  ReadLock(m_ritLock);

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
CcnxTunnel::isPrefix(const Name &prefix, const Name &name)
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
CcnxTunnel::setInterestFilter(const Name &prefix, const InterestCallback &interestCallback)
{
  WriteLock(m_ritLock);
  // make sure copy constructor for boost::function works properly
  m_rit.insert(make_pair(prefix, interestCallback));
  return 0;
}

void
CcnxTunnel::clearInterestFilter(const Name &prefix)
{
  WriteLock(m_ritLock);
  // remove all
  m_rit.erase(prefix);
}

TunnelClosure::TunnelClosure(int retry, const DataCallback &dataCallback, CcnxTunnel *tunnel, const Name &originalInterest, const TimeoutCallback &timeoutCallback)
                  : Closure(retry, dataCallback, timeoutCallback)
                  , m_tunnel(tunnel)
                  , m_originalInterest(originalInterest)
{
}

TunnelClosure::TunnelClosure(const Closure *closure, CcnxTunnel *tunnel, const Name &originalInterest)
                 : Closure(*closure)
                 , m_tunnel(tunnel)
{
}

void
TunnelClosure::runDataCallback(const Name &name, const Bytes &content)
{
  if (m_tunnel != NULL)
  {
    m_tunnel->handleTunneledData(name, content, (*m_dataCallback));
  }
}

Closure::TimeoutCallbackReturnValue
TunnelClosure::runTimeoutCallback(const Name &interest)
{
  return Closure::runTimeoutCallback(m_originalInterest);
}

} // Ccnx
