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

#include "hash-helper.h"

#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <openssl/evp.h>
#include <fstream>

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;
typedef boost::error_info<struct tag_errmsg, int> errmsg_info_int;

#include <boost/archive/iterators/transform_width.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/archive/iterators/dataflow_exception.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace boost;
using namespace boost::archive::iterators;
using namespace std;
namespace fs = boost::filesystem;

template<class CharType>
struct hex_from_4_bit
{
  typedef CharType result_type;
  CharType operator () (CharType ch) const
  {
    const char *lookup_table = "0123456789abcdef";
    // cout << "New character: " << (int) ch << " (" << (char) ch << ")" << "\n";
    BOOST_ASSERT (ch < 16);
    return lookup_table[static_cast<size_t>(ch)];
  }
};

typedef transform_iterator<hex_from_4_bit<string::const_iterator::value_type>,
                           transform_width<string::const_iterator, 4, 8, string::const_iterator::value_type> > string_from_binary;


template<class CharType>
struct hex_to_4_bit
{
  typedef CharType result_type;
  CharType operator () (CharType ch) const
  {
    const signed char lookup_table [] = {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
      -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    // cout << "New character: " << hex << (int) ch << " (" << (char) ch << ")" << "\n";
    signed char value = -1;
    if ((unsigned)ch < 128)
      value = lookup_table [(unsigned)ch];
    if (value == -1)
      BOOST_THROW_EXCEPTION (Error::HashConversion () << errmsg_info_int ((int)ch));

    return value;
  }
};

typedef transform_width<transform_iterator<hex_to_4_bit<string::const_iterator::value_type>, string::const_iterator>, 8, 4> string_to_binary;


std::ostream &
operator << (std::ostream &os, const Hash &hash)
{
  if (hash.m_length == 0)
    return os;

  ostreambuf_iterator<char> out_it (os); // ostream iterator
  // need to encode to base64
  copy (string_from_binary (reinterpret_cast<const char*> (hash.m_buf)),
        string_from_binary (reinterpret_cast<const char*> (hash.m_buf+hash.m_length)),
        out_it);

  return os;
}

std::string
Hash::shortHash () const
{
  return lexical_cast<string> (*this).substr (0, 10);
}


unsigned char Hash::_origin = 0;
HashPtr Hash::Origin(new Hash(&Hash::_origin, sizeof(unsigned char)));

HashPtr
Hash::FromString (const std::string &hashInTextEncoding)
{
  HashPtr retval = make_shared<Hash> (reinterpret_cast<void*> (0), 0);

  if (hashInTextEncoding.size () == 0)
    {
      return retval;
    }

  if (hashInTextEncoding.size () > EVP_MAX_MD_SIZE * 2)
    {
      cerr << "Input hash is too long. Returning an empty hash" << endl;
      return retval;
    }

  retval->m_buf = new unsigned char [EVP_MAX_MD_SIZE];

  unsigned char *end = copy (string_to_binary (hashInTextEncoding.begin ()),
                            string_to_binary (hashInTextEncoding.end ()),
                            retval->m_buf);

  retval->m_length = end - retval->m_buf;

  return retval;
}

HashPtr
Hash::FromFileContent (const fs::path &filename)
{
  HashPtr retval = make_shared<Hash> (reinterpret_cast<void*> (0), 0);
  retval->m_buf = new unsigned char [EVP_MAX_MD_SIZE];

  EVP_MD_CTX *hash_context = EVP_MD_CTX_create ();
  EVP_DigestInit_ex (hash_context, HASH_FUNCTION (), 0);

  fs::ifstream iff (filename, std::ios::in | std::ios::binary);
  while (iff.good ())
    {
      char buf[1024];
      iff.read (buf, 1024);
      EVP_DigestUpdate (hash_context, buf, iff.gcount ());
    }

  retval->m_buf = new unsigned char [EVP_MAX_MD_SIZE];

  EVP_DigestFinal_ex (hash_context,
                      retval->m_buf, &retval->m_length);

  EVP_MD_CTX_destroy (hash_context);

  return retval;
}

HashPtr
Hash::FromBytes (const Ccnx::Bytes &bytes)
{
  HashPtr retval = make_shared<Hash> (reinterpret_cast<void*> (0), 0);
  retval->m_buf = new unsigned char [EVP_MAX_MD_SIZE];

  EVP_MD_CTX *hash_context = EVP_MD_CTX_create ();
  EVP_DigestInit_ex (hash_context, HASH_FUNCTION (), 0);

  // not sure whether it's bad to do so if bytes.size is huge
  EVP_DigestUpdate(hash_context, Ccnx::head(bytes), bytes.size());

  retval->m_buf = new unsigned char [EVP_MAX_MD_SIZE];

  EVP_DigestFinal_ex (hash_context,
                      retval->m_buf, &retval->m_length);

  EVP_MD_CTX_destroy (hash_context);

  return retval;
}
