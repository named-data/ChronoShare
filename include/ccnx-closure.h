#ifndef CCNX_CLOSURE_H
#define CCNX_CLOSURE_H

#include "ccnx-common.h"

using namespace std;

namespace Ccnx {

class Closure 
{
public:
  typedef boost::function<void (const string &, const Bytes &)> DataCallback;

  typedef enum
  {
    RESULT_OK,
    RESULT_REEXPRESS
  } TimeoutCallbackReturnValue;

  typedef boost::function<TimeoutCallbackReturnValue (const string &)> TimeoutCallback;

  Closure(int retry, const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback);
  Closure(const Closure &other);
  int getRetry() {return m_retry;}
  void decRetry() { m_retry--;}
  virtual ~Closure();
  virtual void 
  runDataCallback(const string &name, const Bytes &content);
  virtual TimeoutCallbackReturnValue 
  runTimeoutCallback(const string &interest);

protected:
  int m_retry;
  TimeoutCallback *m_timeoutCallback;
  DataCallback *m_dataCallback;  
};

} // Ccnx

#endif
