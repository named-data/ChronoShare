/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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

#ifndef CCNX_COMMON_H
#define CCNX_COMMON_H

extern "C" {
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/uri.h>
#include <ccn/bloom.h>
#include <ccn/signing.h>
}
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/exception/all.hpp>
#include <boost/function.hpp>
#include <string>
#include <sstream>
#include <map>
#include <utility>
#include <string.h>

using namespace std;
namespace Ccnx {
typedef vector<unsigned char> Bytes;
typedef vector<string>Comps;

typedef boost::shared_ptr<Bytes> BytesPtr;

inline
const unsigned char *
head(const Bytes &bytes)
{
  return &bytes[0];
}

inline
unsigned char *
head (Bytes &bytes)
{
  return &bytes[0];
}

// --- Bytes operations start ---
inline void
readRaw(Bytes &bytes, const unsigned char *src, size_t len)
{
  if (len > 0)
  {
    bytes.resize(len);
    memcpy (head (bytes), src, len);
  }
}

inline BytesPtr
readRawPtr (const unsigned char *src, size_t len)
{
  if (len > 0)
    {
      BytesPtr ret (new Bytes (len));
      memcpy (head (*ret), src, len);

      return ret;
    }
  else
    return BytesPtr ();
}

template<class Msg>
inline BytesPtr
serializeMsg(const Msg &msg)
{
  int size = msg->ByteSize ();
  BytesPtr bytes (new Bytes (size));
  msg->SerializeToArray (head(*bytes), size);
  return bytes;
}

template<class Msg>
inline boost::shared_ptr<Msg>
deserializeMsg (const Bytes &bytes)
{
  boost::shared_ptr<Msg> retval (new Msg ());
  retval->ParseFromArray (head (bytes), bytes.size ());
  return retval;
}

// --- Bytes operations end ---

// Exceptions
typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;

} // Ccnx
#endif // CCNX_COMMON_H
