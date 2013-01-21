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

#include "ccnx-pco.h"

namespace Ccnx {

void
ParsedContentObject::init(const unsigned char *data, size_t len)
{
  readRaw(m_bytes, data, len);

  m_comps = ccn_indexbuf_create();
  int res = ccn_parse_ContentObject(head (m_bytes), len, &m_pco, m_comps);
  if (res < 0)
  {
    boost::throw_exception(MisformedContentObjectException());
  }
}

ParsedContentObject::ParsedContentObject(const unsigned char *data, size_t len)
            : m_comps(NULL)
{
  init(data, len);
}

ParsedContentObject::ParsedContentObject(const Bytes &bytes)
            : m_comps(NULL)
{
  init(head(bytes), bytes.size());
}

ParsedContentObject::ParsedContentObject(const ParsedContentObject &other)
            : m_comps(NULL)
{
  init(head(other.m_bytes), other.m_bytes.size());
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
  int res = ccn_content_get_value(head(m_bytes), m_pco.offset[CCN_PCO_E], &m_pco, &content, &len);
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
  int res = ccn_content_get_value(head(m_bytes), m_pco.offset[CCN_PCO_E], &m_pco, &content, &len);
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

}
