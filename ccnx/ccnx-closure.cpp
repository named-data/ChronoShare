#include "ccnx-closure.h"

namespace Ccnx {

Closure::Closure(const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback)
              : m_timeoutCallback(NULL), m_dataCallback(NULL)
{
  m_timeoutCallback = new TimeoutCallback (timeoutCallback);
  m_dataCallback = new DataCallback (dataCallback);
}

Closure::Closure(const Closure &other)
            : m_timeoutCallback(NULL), m_dataCallback(NULL)
{
  m_timeoutCallback = new TimeoutCallback(*(other.m_timeoutCallback));
  m_dataCallback = new DataCallback(*(other.m_dataCallback));
}

Closure::~Closure ()
{
  delete m_dataCallback;
  delete m_timeoutCallback;
  m_dataCallback = NULL;
  m_timeoutCallback = NULL;
}

Closure::TimeoutCallbackReturnValue
Closure::runTimeoutCallback(const Name &interest)
{
  if ((*m_timeoutCallback).empty())
  {
    return RESULT_OK;
  }

  return (*m_timeoutCallback)(interest);
}


void
Closure::runDataCallback(const Name &name, const Bytes &content)
{
  if (m_dataCallback != NULL) {
    (*m_dataCallback)(name, content);
  }
}

Closure *
Closure::dup() const
{
  Closure *closure = new Closure(*this);
  return closure;
}

} // Ccnx
