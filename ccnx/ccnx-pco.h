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

#ifndef CCNX_CONTENT_OBJECT_H
#define CCNX_CONTENT_OBJECT_H

#include "ccnx-wrapper.h"
#include "ccnx-common.h"
#include "ccnx-name.h"

namespace Ccnx {

struct MisformedContentObjectException : virtual boost::exception, virtual std::exception { };

class ParsedContentObject
{
public:
  ParsedContentObject(const unsigned char *data, size_t len, bool verified = false);
  ParsedContentObject(const unsigned char *data, const ccn_parsed_ContentObject &pco, bool verified = false);
  ParsedContentObject(const Bytes &bytes, bool verified = false);
  ParsedContentObject(const ParsedContentObject &other, bool verified = false);
  virtual ~ParsedContentObject();

  Bytes
  content() const;

  BytesPtr
  contentPtr() const;

  Name
  name() const;

  inline const Bytes &
  buf () const;

  bool
  verified() const { return m_verified; }

  void
  setVerified(bool verified) { m_verified = verified; }

  const unsigned char *
  msg() const { return head(m_bytes); }

  const ccn_parsed_ContentObject *
  pco() const { return &m_pco; }

private:
  void
  init(const unsigned char *data, size_t len);

protected:
  ccn_parsed_ContentObject m_pco;
  ccn_indexbuf *m_comps;
  Bytes m_bytes;
  bool m_verified;
};

const Bytes &
ParsedContentObject::buf () const
{
  return m_bytes;
}


typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

}

#endif // CCNX_CONTENT_OBJECT_H
