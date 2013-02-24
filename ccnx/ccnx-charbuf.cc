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

#include "ccnx-charbuf.h"

using namespace std;

namespace Ccnx {

void
CcnxCharbuf::init(ccn_charbuf *buf)
{
  if (buf != NULL)
  {
    m_buf = ccn_charbuf_create();
    ccn_charbuf_reserve(m_buf, buf->length);
    memcpy(m_buf->buf, buf->buf, buf->length);
    m_buf->length = buf->length;
  }
}

CcnxCharbuf::CcnxCharbuf()
            : m_buf(NULL)
{
  m_buf = ccn_charbuf_create();
}

CcnxCharbuf::CcnxCharbuf(ccn_charbuf *buf)
            : m_buf(NULL)
{
  init(buf);
}

CcnxCharbuf::CcnxCharbuf(const CcnxCharbuf &other)
            : m_buf (NULL)
{
  init(other.m_buf);
}

CcnxCharbuf::CcnxCharbuf(const void *buf, size_t length)
{
  m_buf = ccn_charbuf_create ();
  ccn_charbuf_reserve (m_buf, length);
  memcpy (m_buf->buf, buf, length);
  m_buf->length = length;
}

CcnxCharbuf::~CcnxCharbuf()
{
  ccn_charbuf_destroy(&m_buf);
}

}
