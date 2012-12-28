#ifndef CCNX_WRAPPER_H
#define CCNX_WRAPPER_H

extern "C" {
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/uri.h>
#include <ccn/bloom.h>
#include <ccn/signing.h>
}

#include <vector>
#include <boost/exception/all.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>
#include <string>
#include <sstream>
#include <map>

using namespace std;

namespace Ccnx {

typedef vector<unsigned char> Bytes;

void
readRaw(Bytes &bytes, const unsigned char *src, size_t len);

struct CcnxOperationException : virtual boost::exception, virtual exception { };

class CcnxWrapper {
public:
  typedef boost::function<void (const string &, const Bytes &)> DataCallback;
  typedef boost::function<void (const string &)> InterestCallback;
  typedef enum
  {
    RESULT_OK,
    RESULT_REEXPRESS
  } TimeoutCallbackReturnValue;
  typedef boost::function<TimeoutCallbackReturnValue (const string &)> TimeoutCallback;

public:

  CcnxWrapper();
  virtual ~CcnxWrapper();

  virtual int
  setInterestFilter (const string &prefix, const InterestCallback &interestCallback);

  virtual void
  clearInterestFilter (const string &prefix);

  virtual int 
  sendInterest (const string &strInterest, const DataCallback &dataCallback, int retry = 0, const TimeoutCallback &timeoutCallback = TimeoutCallback());

  virtual int 
  publishData (const string &name, const char *buf, size_t len, int freshness);

  int
  publishData (const string &name, const Bytes &content, int freshness);

  static string
  getLocalPrefix ();

  static string
  extractName(const unsigned char *data, const ccn_indexbuf *comps);

protected:
  Bytes
  createContentObject(const string &name, const char *buf, size_t len, int freshness);

  int 
  putToCcnd (const Bytes &contentObject);
  
protected:
  void
  connectCcnd();

  /// @cond include_hidden 
  void
  createKeyLocator ();

  void
  initKeyStore ();

  const ccn_pkey *
  getPrivateKey ();

  const unsigned char *
  getPublicKeyDigest ();

  ssize_t
  getPublicKeyDigestLength ();

  void
  ccnLoop ();

  int 
  sendInterest (const string &strInterest, void *dataPass);
  /// @endcond

protected:
  ccn* m_handle;
  ccn_keystore *m_keyStore;
  ccn_charbuf *m_keyLoactor;
  // to lock, use "boost::recursive_mutex::scoped_lock scoped_lock(mutex);
  boost::recursive_mutex m_mutex;
  boost::thread m_thread;
  bool m_running;
  bool m_connected;
  map<string, InterestCallback> m_registeredInterests;
  // std::list< std::pair<std::string, InterestCallback> > m_registeredInterests;
};

typedef boost::shared_ptr<CcnxWrapper> CcnxWrapperPtr;

class ClosurePass 
{
public:
  ClosurePass(int retry, const CcnxWrapper::DataCallback &dataCallback, const CcnxWrapper::TimeoutCallback &timeoutCallback);
  int getRetry() {return m_retry;}
  void decRetry() { m_retry--;}
  virtual ~ClosurePass();
  virtual void runDataCallback(const string &name, const Bytes &content);
  virtual CcnxWrapper::TimeoutCallbackReturnValue runTimeoutCallback(const string &interest);

protected:
  int m_retry;
  CcnxWrapper::TimeoutCallback *m_timeoutCallback;
  CcnxWrapper::DataCallback *m_dataCallback;  
};


} // Ccnx

#endif 
