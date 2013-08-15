/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ndnx-pco.h"
#include "ndnx-cert.h"
#include "hash-helper.h"

namespace Ndnx {

void
ParsedContentObject::init(const unsigned char *data, size_t len)
{
  readRaw(m_bytes, data, len);

  m_comps = ndn_indexbuf_create();
  int res = ndn_parse_ContentObject(head (m_bytes), len, &m_pco, m_comps);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

}

ParsedContentObject::ParsedContentObject(const unsigned char *data, size_t len, bool verified)
            : m_comps(NULL)
            , m_verified(verified)
{
  init(data, len);
}

ParsedContentObject::ParsedContentObject(const Bytes &bytes, bool verified)
            : m_comps(NULL)
            , m_verified(verified)
{
  init(head(bytes), bytes.size());
}

ParsedContentObject::ParsedContentObject(const ParsedContentObject &other, bool verified)
            : m_comps(NULL)
            , m_verified(verified)
{
  init(head(other.m_bytes), other.m_bytes.size());
}

ParsedContentObject::~ParsedContentObject()
{
  ndn_indexbuf_destroy(&m_comps);
  m_comps = NULL;
}

Bytes
ParsedContentObject::content() const
{
  const unsigned char *content;
  size_t len;
  int res = ndn_content_get_value(head(m_bytes), m_pco.offset[NDN_PCO_E], &m_pco, &content, &len);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

  Bytes bytes;
  readRaw(bytes, content, len);
  return bytes;
}

BytesPtr
ParsedContentObject::contentPtr() const
{
  const unsigned char *content;
  size_t len;
  int res = ndn_content_get_value(head(m_bytes), m_pco.offset[NDN_PCO_E], &m_pco, &content, &len);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }

  return readRawPtr (content, len);
}

Name
ParsedContentObject::name() const
{
  return Name(head(m_bytes), m_comps);
}

Name
ParsedContentObject::keyName() const
{
  if (m_pco.offset[NDN_PCO_E_KeyName_Name] > m_pco.offset[NDN_PCO_B_KeyName_Name])
  {
    NdnxCharbufPtr ptr = boost::make_shared<NdnxCharbuf>();
    ndn_charbuf_append(ptr->getBuf(), head(m_bytes) + m_pco.offset[NDN_PCO_B_KeyName_Name], m_pco.offset[NDN_PCO_E_KeyName_Name] - m_pco.offset[NDN_PCO_B_KeyName_Name]);

    return Name(*ptr);
}
  else
  {
    return Name();
  }
}

HashPtr
ParsedContentObject::publisherPublicKeyDigest() const
{
  const unsigned char *buf = NULL;
  size_t size = 0;
  ndn_ref_tagged_BLOB(NDN_DTAG_PublisherPublicKeyDigest, head(m_bytes), m_pco.offset[NDN_PCO_B_PublisherPublicKeyDigest], m_pco.offset[NDN_PCO_E_PublisherPublicKeyDigest], &buf, &size);

  return boost::make_shared<Hash>(buf, size);
}

ParsedContentObject::Type
ParsedContentObject::type() const
{
  switch (m_pco.type)
  {
  case NDN_CONTENT_DATA: return DATA;
  case NDN_CONTENT_KEY: return KEY;
  default: break;
  }
  return OTHER;
}

void
ParsedContentObject::verifySignature(const CertPtr &cert)
{
  m_verified = (ndn_verify_signature(head(m_bytes), m_pco.offset[NDN_PCO_E], &m_pco, cert->pkey()) == 1);
}

}
