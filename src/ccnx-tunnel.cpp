#include "ccnx-tunnel.h"
#include "ccnx-pco.h"

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

void
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
CcnxTunnel::sendInterest (const string &interest, Closure *closure)
{
  string strInterest = interest;
  string tunneledInterest = queryRoutableName(strInterest);
  Closure *cp = new TunnelClosure(closure, this, strInterest);
  sendInterest(tunneledInterest, cp);
}

void
CcnxTunnel::handleTunneledData(const string &name, const Bytes &tunneledData, const Closure::DataCallback &originalDataCallback)
{
  ParsedContentObject pco(tunneledData);
  originalDataCallback(pco.name(), pco.content());
}

int
CcnxTunnel::publishData(const string &name, const unsigned char *buf, size_t len, int freshness)
{
  Bytes content = createContentObject(name, buf, len, freshness);
  storeContentObject(name, content);

  return publishContentObject(name, content, freshness);
}

int
CcnxTunnel::publishContentObject(const string &name, const Bytes &contentObject, int freshness)
{
  string tunneledName = m_localPrefix + name;
  Bytes tunneledCo = createContentObject(tunneledName, head(contentObject), contentObject.size(), freshness);
  return putToCcnd(tunneledCo);
}

void
CcnxTunnel::handleTunneledInterest(const string &tunneledInterest)
{
  // The interest must have m_localPrefix as a prefix (component-wise), otherwise ccnd would not deliver it to us
  string interest = (m_localPrefix == "/")
                    ? tunneledInterest
                    : tunneledInterest.substr(m_localPrefix.size());

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
CcnxTunnel::isPrefix(const string &prefix, const string &name)
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
  // make sure copy constructor for boost::function works properly
  m_rit.insert(make_pair(prefix, interestCallback));
  return 0;
}

void
CcnxTunnel::clearInterestFilter(const string &prefix)
{
  WriteLock(m_ritLock);
  // remove all
  m_rit.erase(prefix);
}

TunnelClosure::TunnelClosure(int retry, const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback, CcnxTunnel *tunnel, const string &originalInterest)
                  : Closure(retry, dataCallback, timeoutCallback)
                  , m_tunnel(tunnel)
                  , m_originalInterest(originalInterest)
{
}

TunnelClosure::TunnelClosure(const Closure *closure, CcnxTunnel *tunnel, const string &originalInterest)
                 : Closure(*closure)
                 , m_tunnel(tunnel)
{
}

void
TunnelClosure::runDataCallback(const string &name, const Bytes &content)
{
  if (m_tunnel != NULL)
  {
    m_tunnel->handleTunneledData(name, content, (*m_dataCallback));
  }
}

Closure::TimeoutCallbackReturnValue
TunnelClosure::runTimeoutCallback(const string &interest)
{
  return Closure::runTimeoutCallback(m_originalInterest);
}

} // Ccnx
