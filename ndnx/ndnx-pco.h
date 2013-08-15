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

#ifndef NDNX_CONTENT_OBJECT_H
#define NDNX_CONTENT_OBJECT_H

#include "ndnx-wrapper.h"
#include "ndnx-common.h"
#include "ndnx-name.h"

class Hash;
typedef boost::shared_ptr<Hash> HashPtr;

namespace Ndnx {

struct MisformedContentObjectException : virtual boost::exception, virtual std::exception { };

class Cert;
typedef boost::shared_ptr<Cert> CertPtr;

class ParsedContentObject
{
public:
  enum Type
  {
    DATA,
    KEY,
    OTHER
  };
  ParsedContentObject(const unsigned char *data, size_t len, bool verified = false);
  ParsedContentObject(const unsigned char *data, const ndn_parsed_ContentObject &pco, bool verified = false);
  ParsedContentObject(const Bytes &bytes, bool verified = false);
  ParsedContentObject(const ParsedContentObject &other, bool verified = false);
  virtual ~ParsedContentObject();

  Bytes
  content() const;

  BytesPtr
  contentPtr() const;

  Name
  name() const;

  Name
  keyName() const;

  HashPtr
  publisherPublicKeyDigest() const;

  Type
  type() const;

  inline const Bytes &
  buf () const;

  bool
  verified() const { return m_verified; }

  void
  verifySignature(const CertPtr &cert);

  const unsigned char *
  msg() const { return head(m_bytes); }

  const ndn_parsed_ContentObject *
  pco() const { return &m_pco; }

private:
  void
  init(const unsigned char *data, size_t len);

protected:
  ndn_parsed_ContentObject m_pco;
  ndn_indexbuf *m_comps;
  Bytes m_bytes;
  bool m_verified;
  bool m_integrityChecked;
};

const Bytes &
ParsedContentObject::buf () const
{
  return m_bytes;
}


typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

}

#endif // NDNX_CONTENT_OBJECT_H
