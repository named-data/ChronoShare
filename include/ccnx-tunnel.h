#ifndef CCNX_TUNNEL_H
#define CCNX_TUNNEL_H

#include <string>
#include <boost/function.hpp>
#include <boost/variant.hpp>
#include <boost/thread/locks.hpp>
#include <utility>
#include <map>

#include "ccnx-wrapper.h"

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
  typedef multimap<string, InterestCallback> RegisteredInterestTable;
  typedef multimap<string, InterestCallback>::iterator RitIter;
  typedef boost::shared_mutex Lock;
  typedef boost::unique_lock<Lock> WriteLock;
  typedef boost::shared_lock<Lock> ReadLock;


  CcnxTunnel(); 
  virtual ~CcnxTunnel();

  // name is topology-independent
  virtual int
  publishData(const string &name, const char *buf, size_t len, int freshness) _OVERRIDE;

  virtual int
  sendInterest (const std::string &interest, const DataCallback &dataCallback, int retry = 0, const TimeoutCallback &timeoutCallback = TimeoutCallback()) _OVERRIDE;


  // prefix is topology-independent
  virtual int
  setInterestFilter(const string &prefix, const InterestCallback &interestCallback) _OVERRIDE;

  // prefix is topology-independent
  // this clears all entries with key equal to prefix
  virtual void
  clearInterestFilter(const string &prefix) _OVERRIDE;

  // subclass should provide translation service from topology-independent name
  // to routable name
  virtual string
  queryRoutableName(const string &name) = 0;

  // subclass should implement the function to store ContentObject with topoloy-independent
  // name to the permanent storage; default does nothing
  virtual void
  storeContentObject(const string &name, const Bytes &content) {}

  // should be called  when connect to a different network
  void
  refreshLocalPrefix();

  static bool
  isPrefix(const string &prefix, const string &name);

  void
  handleTunneledInterest(const string &tunneldInterest);
  
  void
  handleTunneledData(const string &name, const Bytes &tunneledData, const DataCallback &originalDataCallback);

protected:
  // need a way to update local prefix, perhaps using macports trick, but eventually we need something more portable
  string m_localPrefix;
  RegisteredInterestTable m_rit;
  Lock m_ritLock;
};

class TunnelClosurePass : public ClosurePass
{
public:
  TunnelClosurePass(int retry, const CcnxWrapper::DataCallback &dataCallback, const CcnxWrapper::TimeoutCallback &timeoutCallback, CcnxTunnel *tunnel, const string &originalInterest);

  virtual void 
  runDataCallback(const string &name, const Bytes &content) _OVERRIDE;

  virtual CcnxWrapper::TimeoutCallbackReturnValue
  runTimeoutCallback(const string &interest) _OVERRIDE;

private:
  CcnxTunnel *m_tunnel;
  string m_originalInterest;
};

};

#endif
