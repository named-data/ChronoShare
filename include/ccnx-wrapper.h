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

#include <boost/thread/locks.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>

#include "ccnx-common.h"
#include "ccnx-interest.h"
#include "ccnx-closure.h"

using namespace std;

namespace Ccnx {

struct CcnxOperationException : virtual boost::exception, virtual exception { };

class CcnxWrapper
{
public:
  typedef boost::shared_mutex Lock;
  typedef boost::unique_lock<Lock> WriteLock;
  typedef boost::shared_lock<Lock> ReadLock;

  typedef boost::recursive_mutex RecLock;
  typedef boost::unique_lock<RecLock> UniqueRecLock;

  typedef boost::function<void (const string &)> InterestCallback;

  CcnxWrapper();
  virtual ~CcnxWrapper();

  virtual int
  setInterestFilter (const string &prefix, const InterestCallback &interestCallback);

  virtual void
  clearInterestFilter (const string &prefix);

  virtual int
  sendInterest (const Interest &interest, Closure *closure);

  virtual int
  publishData (const string &name, const unsigned char *buf, size_t len, int freshness);

  int
  publishData (const string &name, const Bytes &content, int freshness);

  static string
  getLocalPrefix ();

  static string
  extractName(const unsigned char *data, const ccn_indexbuf *comps);

protected:
  Bytes
  createContentObject(const string &name, const unsigned char *buf, size_t len, int freshness);

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

  /// @endcond

protected:
  ccn* m_handle;
  ccn_keystore *m_keyStore;
  ccn_charbuf *m_keyLoactor;
  // to lock, use "boost::recursive_mutex::scoped_lock scoped_lock(mutex);
  RecLock m_mutex;
  boost::thread m_thread;
  bool m_running;
  bool m_connected;
  map<string, InterestCallback> m_registeredInterests;
  // std::list< std::pair<std::string, InterestCallback> > m_registeredInterests;
};

typedef boost::shared_ptr<CcnxWrapper> CcnxWrapperPtr;


} // Ccnx

#endif
