#ifndef CCNX_CLOSURE_H
#define CCNX_CLOSURE_H

#include "ccnx-common.h"
#include "ccnx-name.h"

using namespace std;

namespace Ccnx {

class Closure
{
public:
  typedef boost::function<void (const Name &, const Bytes &)> DataCallback;

  typedef enum
  {
    RESULT_OK,
    RESULT_REEXPRESS
  } TimeoutCallbackReturnValue;

  typedef boost::function<TimeoutCallbackReturnValue (const Name &)> TimeoutCallback;

  Closure(const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback = TimeoutCallback());
  Closure(const Closure &other);
  virtual ~Closure();
  virtual void
  runDataCallback(const Name &name, const Bytes &content);
  virtual TimeoutCallbackReturnValue
  runTimeoutCallback(const Name &interest);

  virtual Closure *
  dup() const;

protected:
  TimeoutCallback *m_timeoutCallback;
  DataCallback *m_dataCallback;
};

} // Ccnx

#endif
