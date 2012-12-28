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
  ParsedContentObject((const unsigned char *)bytes[0], bytes.size());
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
ParsedContentObject::content()
{
  const unsigned char *content;
  size_t len;
  Bytes bytes;
  int res = ccn_content_get_value((const unsigned char *)m_bytes[0], m_pco.offset[CCN_PCO_E], &m_pco, &content, &len);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

  readRaw(bytes, content, len);
  return bytes;
}

string
ParsedContentObject::name()
{
  return CcnxWrapper::extractName((const unsigned char *)m_bytes[0], m_comps);
}

}
