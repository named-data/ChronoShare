#ifndef CCNX_CONTENT_OBJECT_H
#define CCNX_CONTENT_OBJECT_H

#include "ccnx-wrapper.h"
#include "ccnx-common.h"
#include "ccnx-name.h"

using namespace std;

namespace Ccnx {

struct MisformedContentObjectException : virtual boost::exception, virtual exception { };

class ParsedContentObject
{
public:
  ParsedContentObject(const unsigned char *data, size_t len);
  ParsedContentObject(const Bytes &bytes);
  ParsedContentObject(const ParsedContentObject &other);
  virtual ~ParsedContentObject();

  Bytes
  content() const;

  Name
  name() const;

protected:
  Name m_name;
  Bytes m_content;
};

typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

}

#endif // CCNX_CONTENT_OBJECT_H
