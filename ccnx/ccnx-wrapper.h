#ifndef CCNX_WRAPPER_H
#define CCNX_WRAPPER_H

#include <boost/thread/locks.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>

#include "ccnx-common.h"
#include "ccnx-name.h"
#include "ccnx-selectors.h"
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

  typedef boost::function<void (const Name &)> InterestCallback;

  CcnxWrapper();
  virtual ~CcnxWrapper();

  virtual int
  setInterestFilter (const Name &prefix, const InterestCallback &interestCallback);

  virtual void
  clearInterestFilter (const Name &prefix);

  virtual int
  sendInterest (const Name &interest, const Closure *closure, const Selectors &selector = Selectors());

  virtual int
  publishData (const Name &name, const unsigned char *buf, size_t len, int freshness);

  int
  publishData (const Name &name, const Bytes &content, int freshness);

  static Name
  getLocalPrefix ();

  Bytes
  createContentObject(const Name &name, const void *buf, size_t len, int freshness = 2147/* max value for ccnx*/);

  int
  putToCcnd (const Bytes &contentObject);

private:
  CcnxWrapper(const CcnxWrapper &other) {}

protected:
  void
  connectCcnd();

  /// @cond include_hidden
  void
  ccnLoop ();

  /// @endcond

protected:
  ccn* m_handle;
  RecLock m_mutex;
  boost::thread m_thread;
  bool m_running;
  bool m_connected;
  map<Name, InterestCallback> m_registeredInterests;
};

typedef boost::shared_ptr<CcnxWrapper> CcnxWrapperPtr;


} // Ccnx

#endif
