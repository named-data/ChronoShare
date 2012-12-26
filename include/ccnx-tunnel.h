#ifndef CCNX_TUNNEL_H
#define CCNX_TUNNEL_H

#include <sync/sync-ccnx-wrapper.h>
#include <string>
#include <boost/function.hpp>
#include <boost/variant.hpp>
#include <boost/thread/locks.hpp>
#include <utility>
#include <map>

using namespace Sync;
using namespace std;

// Eventually, Sync::CcnxWrapper should be moved to this namespace. 
// It has nothing to do with Sync.
namespace Ccnx
{

class CcnxTunnel : public CcnxWrapper 
{
public: 
  typedef boost::variant<StringDataCallback, RawDataCallback> DataCallback;
  typedef multimap<string, DataCallback> PendingInterestTable;
  typedef multimap<string, InterestCallback> RegisteredInterestTable;
  typedef multimap<string, DataCallback>iterator PitIter;
  typedef multimap<string, InterestCallback>iterator RitIter;
  typedef boost::shared_mutex Lock;
  typedef boost::unique_lock<Lock> WriteLock;
  typedef boost::shared_lock<Lock> ReadLock;


  CcnxTunnel(); 
  virtual ~CcnxTunnel();

  // name is topology-independent
  virtual int
  publishRawData(const string &name, const char *buf, size_t len, int freshness);

  // prefix is topology-independent
  virtual int
  setInterestFilter(const string &prefix, const InterestCallback &interestCallback);

  // prefix is topology-independent
  // this clears all entries with key equal to prefix
  virtual void
  clearInterestFilter(const string &prefix);

  // subclass should provide translation service from topology-independent name
  // to routable name
  virtual string
  queryRoutableName(string &name) = 0;

  // subclass should implement the function to store ContentObject with topoloy-independent
  // name to the permanent storage; default does nothing
  virtual void
  storeContentObject(string &name, ContentObjectPtr co) {}

  // should be called  when connect to a different network
  void
  refreshLocalPrefix();

  static bool
  isPrefix(string &prefix, string &name);

protected:
  void
  handleTunneledInterest(string tunneldInterest);

  virtual int
  sendInterest (const std::string &strInterest, void *dataPass);

protected:
  // need a way to update local prefix, perhaps using macports trick, but eventually we need something more portable
  string m_localPrefix;
  PendingInterestTable m_pit;
  RegisteredInterestTable m_rit;
  Lock m_pitLock;
  Lock m_ritLock;
};

};

#endif
