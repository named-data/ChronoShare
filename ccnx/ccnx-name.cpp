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

#include "ccnx-name.h"
#include <boost/lexical_cast.hpp>
#include <ctype.h>
#include <boost/algorithm/string/join.hpp>

using namespace std;

namespace Ccnx{

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

Name::Name()
{
}

Name::Name(const string &name)
{
  stringstream ss(name);
  string compStr;
  bool first = true;
  while(getline(ss, compStr, '/'))
  {
    // discard the first empty comp before the first '/'
    if (first)
    {
      first = false;
      continue;
    }
    Bytes comp(compStr.begin(), compStr.end());
    m_comps.push_back(comp);
  }
}

Name::Name(const vector<Bytes> &comps)
{
  m_comps = comps;
}

Name::Name(const Name &other)
{
  m_comps = other.m_comps;
}

Name::Name(const unsigned char *data, const ccn_indexbuf *comps)
{
  for (int i = 0; i < comps->n - 1; i++)
  {
    const unsigned char *compPtr;
    size_t size;
    ccn_name_comp_get(data, comps, i, &compPtr, &size);
    Bytes comp;
    readRaw(comp, compPtr, size);
    m_comps.push_back(comp);
  }
}

Name::Name (const void *buf, const size_t length)
{
  ccn_indexbuf *idx = ccn_indexbuf_create();
  const ccn_charbuf namebuf = { length, length, const_cast<unsigned char *> (reinterpret_cast<const unsigned char *> (buf)) };
  ccn_name_split (&namebuf, idx);

  const unsigned char *compPtr = NULL;
  size_t size = 0;
  int i = 0;
  while (ccn_name_comp_get(namebuf.buf, idx, i, &compPtr, &size) == 0)
    {
      Bytes comp;
      readRaw (comp, compPtr, size);
      m_comps.push_back(comp);
      i++;
    }
  ccn_indexbuf_destroy(&idx);
}

Name::Name (const CcnxCharbuf &buf)
{
  ccn_indexbuf *idx = ccn_indexbuf_create();
  ccn_name_split (buf.getBuf (), idx);

  const unsigned char *compPtr = NULL;
  size_t size = 0;
  int i = 0;
  while (ccn_name_comp_get(buf.getBuf ()->buf, idx, i, &compPtr, &size) == 0)
    {
      Bytes comp;
      readRaw (comp, compPtr, size);
      m_comps.push_back(comp);
      i++;
    }
  ccn_indexbuf_destroy(&idx);
}

Name::Name (const ccn_charbuf *buf)
{
  ccn_indexbuf *idx = ccn_indexbuf_create();
  ccn_name_split (buf, idx);

  const unsigned char *compPtr = NULL;
  size_t size = 0;
  int i = 0;
  while (ccn_name_comp_get(buf->buf, idx, i, &compPtr, &size) == 0)
    {
      Bytes comp;
      readRaw (comp, compPtr, size);
      m_comps.push_back(comp);
      i++;
    }
  ccn_indexbuf_destroy(&idx);
}

Name &
Name::operator=(const Name &other)
{
  m_comps = other.m_comps;
  return *this;
}
bool
Name::operator==(const string &str) const
{
  return this->toString() == str;
}

bool
Name::operator!=(const string &str) const
{
  return !(*this == str);
}

Name
operator+(const Name &n1, const Name &n2)
{
  vector<Bytes> comps = n1.m_comps;
  copy(n2.m_comps.begin(), n2.m_comps.end(), back_inserter(comps));
  return Name(comps);
}

string
Name::toString() const
{
  stringstream ss(stringstream::out);
  ss << *this;
  return ss.str();
}

CcnxCharbufPtr
Name::toCcnxCharbuf() const
{
  CcnxCharbufPtr ptr(new CcnxCharbuf());
  ccn_charbuf *cbuf = ptr->getBuf();
  ccn_name_init(cbuf);
  int size = m_comps.size();
  for (int i = 0; i < size; i++)
  {
    ccn_name_append(cbuf, head(m_comps[i]), m_comps[i].size());
  }
  return ptr;
}

Name &
Name::appendComp(const Name &comp)
{
  m_comps.insert (m_comps.end (),
                  comp.m_comps.begin (), comp.m_comps.end ());
  return *this;
}

Name &
Name::appendComp(const Bytes &comp)
{
  m_comps.push_back(comp);
  return *this;
}

Name &
Name::appendComp(const string &compStr)
{
  Bytes comp(compStr.begin(), compStr.end());
  return appendComp(comp);
}

Name &
Name::appendComp (const void *buf, size_t size)
{
  Bytes comp (reinterpret_cast<const unsigned char*> (buf), reinterpret_cast<const unsigned char*> (buf) + size);
  return appendComp(comp);
}

Name &
Name::appendComp(uint64_t number)
{
  Bytes comp;
  comp.push_back (0);

  while (number > 0)
    {
      comp.push_back (static_cast<unsigned char> (number & 0xFF));
      number >>= 8;
    }
  return appendComp (comp);
}

uint64_t
Name::getCompAsInt (int index) const
{
  Bytes comp = getComp(index);
  if (comp.size () < 1 ||
      comp[0] != 0)
    {
      boost::throw_exception(NameException()
                             << error_info_str("Non integer component: " + getCompAsString(index)));
    }
  uint64_t ret = 0;
  for (int i = comp.size () - 1; i >= 1; i--)
    {
      ret <<= 8;
      ret |= comp [i];
    }
  return ret;
}

const Bytes &
Name::getComp(int index) const
{
  if (index >= m_comps.size())
  {
    boost::throw_exception(NameException() << error_info_str("Index out of range: " + boost::lexical_cast<string>(index)));
  }
  return m_comps[index];
}

string
Name::getCompAsString(int index) const
{
  Bytes comp = getComp(index);
  stringstream ss(stringstream::out);
  int size = comp.size();
  for (int i = 0; i < size; i++)
  {
    unsigned char ch = comp[i];
    if (isprint(ch))
    {
      ss << (char) ch;
    }
    else
    {
      ss << "%" << hex << setfill('0') << setw(2) << (unsigned int)ch;
    }
  }

  return ss.str();
}

Name
Name::getPartialName(int start, int n) const
{
  int size = m_comps.size();
  if (start < 0 || start >= size || (n > 0 && start + n > size))
  {
    stringstream ss(stringstream::out);
    ss << "getPartialName() parameter out of range! ";
    ss << "start = " << start;
    ss << "n = " << n;
    ss << "size = " << size;
    boost::throw_exception(NameException() << error_info_str(ss.str()));
  }

  vector<Bytes> comps;
  int end;
  if (n > 0)
  {
    end = start + n;
  }
  else
  {
    end = size;
  }

  for (int i = start; i < end; i++)
  {
    comps.push_back(m_comps[i]);
  }

  return Name(comps);
}

ostream &
operator <<(ostream &os, const Name &name)
{
  int size = name.size();
  vector<string> strComps;
  for (int i = 0; i < size; i++)
  {
    strComps.push_back(name.getCompAsString(i));
  }
  string joined = boost::algorithm::join(strComps, "/");
  os << "/" << joined;
  return os;
}

bool
operator ==(const Name &n1, const Name &n2)
{
  stringstream ss1(stringstream::out);
  stringstream ss2(stringstream::out);
  ss1 << n1;
  ss2 << n2;
  return ss1.str() == ss2.str();
}

bool
operator !=(const Name &n1, const Name &n2)
{
  return !(n1 == n2);
}

bool
operator <(const Name &n1, const Name &n2)
{
  stringstream ss1(stringstream::out);
  stringstream ss2(stringstream::out);
  ss1 << n1;
  ss2 << n2;
  return ss1.str() < ss2.str();
}


} // Ccnx
