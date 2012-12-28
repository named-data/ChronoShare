#ifndef CCNX_CONTENT_OBJECT_H
#define CCNX_CONTENT_OBJECT_H

#include <boost/exception/all.hpp>
#include <boost/shared_ptr.hpp>
#include "ccnx-wrapper.h"

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

  string
  name() const;

protected:
  ccn_parsed_ContentObject m_pco;
  ccn_indexbuf *m_comps;
  Bytes m_bytes;
};

typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

}

#endif // CCNX_CONTENT_OBJECT_H
