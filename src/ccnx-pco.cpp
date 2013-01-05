#include "ccnx-pco.h"

namespace Ccnx {

ParsedContentObject::ParsedContentObject(const unsigned char *data, size_t len)
            : m_comps(NULL)
{
  m_comps = ccn_indexbuf_create();
  int res = ccn_parse_ContentObject(data, len, &m_pco, m_comps);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }
  readRaw(m_bytes, data, len);
}

ParsedContentObject::ParsedContentObject(const Bytes &bytes)
{
  ParsedContentObject(head(bytes), bytes.size());
}

ParsedContentObject::ParsedContentObject(const ParsedContentObject &other)
{
  ParsedContentObject(other.m_bytes);
}

ParsedContentObject::~ParsedContentObject()
{
  ccn_indexbuf_destroy(&m_comps);
  m_comps = NULL;
}

Bytes
ParsedContentObject::content() const
{
  const unsigned char *content;
  size_t len;
  Bytes bytes;
  int res = ccn_content_get_value(head(m_bytes), m_pco.offset[CCN_PCO_E], &m_pco, &content, &len);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

  readRaw(bytes, content, len);
  return bytes;
}

Name
ParsedContentObject::name() const
{
  return Name(head(m_bytes), m_comps);
}

}
