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

#include <boost/exception/all.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/function.hpp>
#include <string>
#include <sstream>
#include <map>

namespace Ccnx {

struct CcnxOperationException : virtual boost::exception, virtual std::exception { };

class CcnxWrapper;

class ContentObject
{
public:
  ContentObject(const char *data, size_t len);
  ~ContentObject();
  void
  dup(char **data, size_t *len);

// exposed! use cautiously at your own risk.
public:
  char *m_data;
  size_t m_len;
};

typedef boost::shared_ptr<ContentObject> ContentObjectPtr;

class CcnxWrapper {
public:
  typedef boost::function<void (std::string, const char *buf, size_t len)> DataCallback;
  typedef boost::function<void (std::string)> InterestCallback;
  typedef boost::function<TimeoutCallbackReturnValue (std::string)> TimeoutCallback;

  typedef enum
  {
    RESULT_OK,
    RESULT_REEXPRESS
  } TimeoutCallbackReturnValue;

public:

  CcnxWrapper();
  virtual ~CcnxWrapper();

  virtual int
  setInterestFilter (const std::string &prefix, const InterestCallback &interestCallback);

  virtual void
  clearInterestFilter (const std::string &prefix);

  virtual int 
  sendInterest (const std::string &strInterest, const RawDataCallback &rawDataCallback, int retry = 0, const TimeoutCallback &timeoutCallback = TimeoutCallback());

  virtual int 
  publishData (const std::string &name, const char *buf, size_t len, int freshness);

  ContentObjectPtr 
  createContentObject(const std::string &name, const char *buf, size_t len, int freshness);

  int 
  putToCcnd (ContentObjectPtr co);

  static std::string
  getInterestName(ccn_upcall_info *info);

  static std::string
  getDataName(ccn_upcall_info *info);

  static int
  getContentFromContentObject(ContentObjectPtr co, char **data, size_t *len);

  static std::string
  getLocalPrefix ();
  
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
  sendInterest (const std::string &strInterest, void *dataPass);
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
  std::map<std::string, InterestCallback> m_registeredInterests;
  // std::list< std::pair<std::string, InterestCallback> > m_registeredInterests;
};

typedef boost::shared_ptr<CcnxWrapper> CcnxWrapperPtr;

class ClosurePass {
public:
  ClosurePass(int retry, const CcnxWrapper::DataCallback &dataCallback, const CcnxWrapper::TimeoutCallback &timeoutCallback);
  int getRetry() {return m_retry;}
  void decRetry() { m_retry--;}
  virtual ~ClosurePass(){}
  virtual void runCallback(std::string name, const char *data, size_t len);
  virtual CcnxWrapper::TimeoutCallbackReturnValue runTimeoutCallback(std::string interest);

protected:
  int m_retry;
  CallbackType m_type;
  CcnxWrapper::TimeoutCallback *m_timeoutCallback;
  CcnxWrapper::DataCallback *m_dataCallback;  
};


} // Ccnx

#endif 
