#ifndef CCNX_TUNNEL_H
#define CCNX_TUNNEL_H

#include <boost/variant.hpp>

#include "ccnx-common.h"
#include "ccnx-wrapper.h"
#include "ccnx-name.h"
#include "ccnx-selectors.h"

#define _OVERRIDE
#ifdef __GNUC__
#if __GNUC_MAJOR >= 4 && __GNUC_MINOR__ >= 7
  #undef _OVERRIDE
  #define _OVERRIDE override
#endif // __GNUC__ version
#endif // __GNUC__

using namespace std;

// Eventually, Sync::CcnxWrapper should be moved to this namespace.
// It has nothing to do with Sync.
namespace Ccnx
{

class CcnxTunnel : public CcnxWrapper
{
public:
  typedef multimap<Name, InterestCallback> RegisteredInterestTable;
  typedef multimap<Name, InterestCallback>::iterator RitIter;


  CcnxTunnel();
  virtual ~CcnxTunnel();

  // name is topology-independent
  virtual int
  publishData(const Name &name, const unsigned char *buf, size_t len, int freshness) _OVERRIDE;

  int
  publishContentObject(const Name &name, const Bytes &contentObject, int freshness);

  virtual int
  sendInterest (const Name &interest, const Closure *closure, const Selectors &selectors = Selectors());


  // prefix is topology-independent
  virtual int
  setInterestFilter(const Name &prefix, const InterestCallback &interestCallback) _OVERRIDE;

  // prefix is topology-independent
  // this clears all entries with key equal to prefix
  virtual void
  clearInterestFilter(const Name &prefix) _OVERRIDE;

  // subclass should provide translation service from topology-independent name
  // to routable name
  virtual Name
  queryRoutableName(const Name &name) = 0;

  // subclass should implement the function to store ContentObject with topoloy-independent
  // name to the permanent storage; default does nothing
  virtual void
  storeContentObject(const Name &name, const Bytes &content) {}

  // should be called  when connect to a different network
  void
  refreshLocalPrefix();

  static bool
  isPrefix(const Name &prefix, const Name &name);

  void
  handleTunneledInterest(const Name &tunneldInterest);

  void
  handleTunneledData(const Name &name, const Bytes &tunneledData, const Closure::DataCallback &originalDataCallback);

private:
  CcnxTunnel(const CcnxTunnel &other) {}

protected:
  // need a way to update local prefix, perhaps using macports trick, but eventually we need something more portable
  Name m_localPrefix;
  RegisteredInterestTable m_rit;
  Lock m_ritLock;
};

class TunnelClosure : public Closure
{
public:
  TunnelClosure(const DataCallback &dataCallback, CcnxTunnel *tunnel, const Name &originalInterest, const TimeoutCallback &timeoutCallback = TimeoutCallback());

  TunnelClosure(const Closure *closure, CcnxTunnel *tunnel, const Name &originalInterest);

  virtual void
  runDataCallback(const Name &name, const Bytes &content) _OVERRIDE;

  virtual TimeoutCallbackReturnValue
  runTimeoutCallback(const Name &interest) _OVERRIDE;

  virtual Closure *
  dup() const _OVERRIDE;

private:
  CcnxTunnel *m_tunnel;
  Name m_originalInterest;
};

};

#endif
