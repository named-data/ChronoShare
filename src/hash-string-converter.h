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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef HASH_STRING_CONVERTER_H
#define HASH_STRING_CONVERTER_H

#include <string.h>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/exception/all.hpp>

class Hash
{
public:
  Hash (const void *buf, size_t length)
    : m_length (length)
  {
    if (m_length != 0)
      {
        m_buf = new unsigned char [length];
        memcpy (m_buf, buf, length);
      }
  }

  Hash (const std::string &hashInTextEncoding);

  ~Hash ()
  {
    if (m_length != 0)
      delete [] m_buf;
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

  const void *
  GetHash () const
  {
    return m_buf;
  }

  size_t
  GetHashBytes () const
  {
    return m_length;
  }
  
private:
  unsigned char *m_buf;
  size_t m_length;

  Hash (const Hash &) { }
  Hash & operator = (const Hash &) { return *this; }
  
  friend std::ostream &
  operator << (std::ostream &os, const Hash &digest);
};

namespace Error {
struct HashConversion : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<Hash> HashPtr;


std::ostream &
operator << (std::ostream &os, const Hash &digest);

#endif // HASH_STRING_CONVERTER_H
