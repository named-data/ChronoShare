#ifndef CCNX_INTEREST_H
#define CCNX_INTEREST_H
#include "ccnx-common.h"

namespace Ccnx {

// Currently, other classes use string when Interest is needed.
// The constructor from string ensures that if we change other classes' APIs to use Interest object 
// instead of string, it is still backwards compatible
// Using a separate Interest class instead of simple string allows us to add control fields
// to the Interest, e.g. ChildSelector. Perhaps that's a separate Selectors class. Will do it after ChronoShare project finishes.

// Since the selector is only useful when sending Interest (in callbacks, usually we only need to know the name of the Interest),
// we currently only use Interest object in sendInterest of CcnxWrapper. In other places, Interest object is equivalent of
// its string name.

class Interest
{
public:
  Interest(const string &name) : m_name(name) {}
  virtual ~Interest() {}

  string
  name() const { return m_name; }

protected:
  string m_name;
};

} // Ccnx
#endif
