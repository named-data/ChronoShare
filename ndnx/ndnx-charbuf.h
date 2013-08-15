/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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
 *	   Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef NDNX_NDNX_CHARBUF_H
#define NDNX_NDNX_CHARBUF_H

#include "ndnx-common.h"
#include <boost/shared_ptr.hpp>

namespace Ndnx {

class NdnxCharbuf;
typedef boost::shared_ptr<NdnxCharbuf> NdnxCharbufPtr;

//  This class is mostly used in NdnxWrapper; users should not be directly using this class
// The main purpose of this class to is avoid manually create and destroy charbuf everytime
class NdnxCharbuf
{
public:
  NdnxCharbuf();
  NdnxCharbuf(ndn_charbuf *buf);
  NdnxCharbuf(const NdnxCharbuf &other);
  NdnxCharbuf(const void *buf, size_t length);
  ~NdnxCharbuf();

  // expose internal data structure, use with caution!!
  ndn_charbuf *
  getBuf() { return m_buf; }

  const ndn_charbuf *
  getBuf() const { return m_buf; }

  const unsigned char *
  buf () const
  { return m_buf->buf; }

  size_t
  length () const
  { return m_buf->length; }

private:
  void init(ndn_charbuf *buf);

protected:
  ndn_charbuf *m_buf;
};

}

#endif // NDNX_NDNX_CHARBUF_H
