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
  ParsedContentObject(const unsigned char *data, size_t len);
  ParsedContentObject(const unsigned char *data, const ccn_parsed_ContentObject &pco);
  ParsedContentObject(const Bytes &bytes);
  ParsedContentObject(const ParsedContentObject &other);
  virtual ~ParsedContentObject();

  Bytes
  content() const;

  BytesPtr
  contentPtr() const;

  Name
  name() const;

private:
  void
  init(const unsigned char *data, size_t len);

protected:
  ccn_parsed_ContentObject m_pco;
  ccn_indexbuf *m_comps;
  Bytes m_bytes;
};

typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

}

#endif // CCNX_CONTENT_OBJECT_H
