#include "ccnx-pco.h"

namespace Ccnx {

ParsedContentObject::ParsedContentObject(const unsigned char *data, size_t len)
{
  ccn_indexbuf *comps = ccn_indexbuf_create();
  ccn_parsed_ContentObject pco;
  int res = ccn_parse_ContentObject(data, len, &pco, comps);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

  const unsigned char *content;
  size_t length;
  res = ccn_content_get_value(data, pco.offset[CCN_PCO_E], &pco, &content, &length);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }
  readRaw(m_content, content, length);

  m_name = Name(data, comps);
  cout << "in Constructor: name " << m_name << endl;
  cout << "content : " << string((const char *)&m_content[0], m_content.size()) << endl;
}

ParsedContentObject::ParsedContentObject(const Bytes &bytes)
{
  ParsedContentObject(head(bytes), bytes.size());
}

ParsedContentObject::ParsedContentObject(const ParsedContentObject &other)
{
  m_content = other.m_content;
  m_name = other.m_name;
}

ParsedContentObject::~ParsedContentObject()
{
}

Bytes
ParsedContentObject::content() const
{
  cout << "content() : " << string((const char *)&m_content[0], m_content.size()) << endl;
  return m_content;
}

Name
ParsedContentObject::name() const
{
  cout <<"name() : " << m_name << endl;
  return m_name;
}

}
