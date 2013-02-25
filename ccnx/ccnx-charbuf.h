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

#ifndef CCNX_CCNX_CHARBUF_H
#define CCNX_CCNX_CHARBUF_H

#include "ccnx-common.h"
#include <boost/shared_ptr.hpp>

namespace Ccnx {

class CcnxCharbuf;
typedef boost::shared_ptr<CcnxCharbuf> CcnxCharbufPtr;

//  This class is mostly used in CcnxWrapper; users should not be directly using this class
// The main purpose of this class to is avoid manually create and destroy charbuf everytime
class CcnxCharbuf
{
public:
  CcnxCharbuf();
  CcnxCharbuf(ccn_charbuf *buf);
  CcnxCharbuf(const CcnxCharbuf &other);
  CcnxCharbuf(const void *buf, size_t length);
  ~CcnxCharbuf();

  // expose internal data structure, use with caution!!
  ccn_charbuf *
  getBuf() { return m_buf; }

  const ccn_charbuf *
  getBuf() const { return m_buf; }

  const unsigned char *
  buf () const
  { return m_buf->buf; }

  size_t
  length () const
  { return m_buf->length; }

private:
  void init(ccn_charbuf *buf);

protected:
  ccn_charbuf *m_buf;
};

}

#endif // CCNX_CCNX_CHARBUF_H
