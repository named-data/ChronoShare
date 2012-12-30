#include "ccnx-closure.h"

namespace Ccnx {

Closure::Closure(int retry, const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback)
              : m_retry(retry), m_timeoutCallback(NULL)
{
  m_timeoutCallback = new TimeoutCallback (timeoutCallback);
  m_dataCallback = new DataCallback (dataCallback);
}

Closure::Closure(const Closure &other)
{
  Closure(other.m_retry, *(other.m_dataCallback), *(other.m_timeoutCallback));
}

Closure::~Closure ()
{
  delete m_dataCallback;
  delete m_timeoutCallback;
  m_dataCallback = NULL;
  m_timeoutCallback = NULL;
}

Closure::TimeoutCallbackReturnValue
Closure::runTimeoutCallback(const string &interest)
{
  if ((*m_timeoutCallback).empty())
  {
    return RESULT_OK;
  }

  return (*m_timeoutCallback)(interest);
}


void
Closure::runDataCallback(const string &name, const Bytes &content)
{
  if (m_dataCallback != NULL) {
    (*m_dataCallback)(name, content);
  }
}

} // Ccnx
