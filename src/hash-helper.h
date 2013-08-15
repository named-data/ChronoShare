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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef HASH_HELPER_H
#define HASH_HELPER_H

#include <string.h>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include "ccnx-common.h"

// Other options: VP_md2, EVP_md5, EVP_sha, EVP_sha1, EVP_sha256, EVP_dss, EVP_dss1, EVP_mdc2, EVP_ripemd160
#define HASH_FUNCTION EVP_sha256

class Hash;
typedef boost::shared_ptr<Hash> HashPtr;

class Hash
{
public:
  static unsigned char _origin;
  static HashPtr Origin;

  Hash ()
    : m_buf(0)
    , m_length(0)
  {
  }

  Hash (const void *buf, unsigned int length)
    : m_length (length)
  {
    if (m_length != 0)
      {
        m_buf = new unsigned char [length];
        memcpy (m_buf, buf, length);
      }
  }

  Hash (const Hash &otherHash)
  : m_length (otherHash.m_length)
  {
    if (m_length != 0)
      {
        m_buf = new unsigned char [m_length];
        memcpy (m_buf, otherHash.m_buf, otherHash.m_length);
      }
  }

  static HashPtr
  FromString (const std::string &hashInTextEncoding);

  static HashPtr
  FromFileContent (const boost::filesystem::path &fileName);

  static HashPtr
  FromBytes (const Ccnx::Bytes &bytes);

  ~Hash ()
  {
    if (m_length != 0)
      delete [] m_buf;
  }

  Hash &
  operator = (const Hash &otherHash)
  {
    if (m_length != 0)
      delete [] m_buf;

    m_length = otherHash.m_length;
    if (m_length != 0)
      {
        m_buf = new unsigned char [m_length];
        memcpy (m_buf, otherHash.m_buf, otherHash.m_length);
      }
    return *this;
  }

  bool
  IsZero () const
  {
    return m_length == 0 ||
      (m_length == 1 && m_buf[0] == 0);
  }

  bool
  operator == (const Hash &otherHash) const
  {
    if (m_length != otherHash.m_length)
      return false;

    return memcmp (m_buf, otherHash.m_buf, m_length) == 0;
  }

  bool operator < (const Hash &otherHash) const
  {
    if (m_length < otherHash.m_length)
      return true;

    if (m_length > otherHash.m_length)
      return false;

    for (unsigned int i = 0; i < m_length; i++)
      {
        if (m_buf [i] < otherHash.m_buf [i])
          return true;

        if (m_buf [i] > otherHash.m_buf [i])
          return false;

        // if equal, continue
      }

    return false;
  }

  const void *
  GetHash () const
  {
    return m_buf;
  }

  unsigned int
  GetHashBytes () const
  {
    return m_length;
  }

  std::string
  shortHash () const;

private:
  unsigned char *m_buf;
  unsigned int m_length;

  friend std::ostream &
  operator << (std::ostream &os, const Hash &digest);
};

namespace Error {
struct HashConversion : virtual boost::exception, virtual std::exception { };
}


std::ostream &
operator << (std::ostream &os, const Hash &digest);

#endif // HASH_STRING_CONVERTER_H
