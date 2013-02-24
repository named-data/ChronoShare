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

#ifndef CCNX_NAME_H
#define CCNX_NAME_H
#include <boost/shared_ptr.hpp>
#include "ccnx-common.h"
#include "ccnx-charbuf.h"

namespace Ccnx {

struct NameException:
    virtual boost::exception, virtual std::exception {};

class Name
{
public:
  Name();
  Name(const std::string &name);
  Name(const std::vector<Bytes> &comps);
  Name(const Name &other);
  Name(const unsigned char *data, const ccn_indexbuf *comps);
  Name (const void *buf, const size_t length);
  Name (const CcnxCharbuf &buf);
  Name (const ccn_charbuf *buf);
  virtual ~Name() {}

  CcnxCharbufPtr
  toCcnxCharbuf() const;

  CcnxCharbuf*
  toCcnxCharbufRaw () const;

  operator CcnxCharbufPtr () const { return toCcnxCharbuf (); }

  Name &
  appendComp(const Name &comp);

  Name &
  appendComp(const Bytes &comp);

  Name &
  appendComp(const std::string &compStr);

  Name &
  appendComp(const void *buf, size_t size);

  /**
   * Append int component
   *
   * Difference between this and appendComp call is that data is appended in network order
   *
   * Also, this function honors naming convention (%00 prefix is added)
   */
  Name &
  appendComp(uint64_t number);

  template<class T>
  Name &
  operator ()(const T &comp) { return appendComp (comp); }

  Name &
  operator ()(const void *buf, size_t size) { return appendComp (buf, size); }

  int
  size() const {return m_comps.size();}

  const Bytes &
  getComp (int index) const;

  inline const Bytes &
  getCompFromBack (int index) const;

  // return std::string format of the comp
  // if all characters are printable, simply returns the string
  // if not, print the bytes in hex string format
  std::string
  getCompAsString(int index) const;

  uint64_t
  getCompAsInt (int index) const;

  inline std::string
  getCompFromBackAsString(int index) const;

  inline uint64_t
  getCompFromBackAsInt (int index) const;

  Name
  getPartialName(int start, int n = -1) const;

  inline Name
  getPartialNameFromBack(int start, int n = -1) const;

  std::string
  toString() const;

  Name &
  operator=(const Name &other);

  bool
  operator==(const std::string &str) const;

  bool
  operator!=(const std::string &str) const;

  friend Name
  operator+(const Name &n1, const Name &n2);

private:
  std::vector<Bytes> m_comps;
};

typedef boost::shared_ptr<Name> NamePtr;

std::ostream&
operator <<(std::ostream &os, const Name &name);

bool
operator ==(const Name &n1, const Name &n2);

bool
operator !=(const Name &n1, const Name &n2);

bool
operator <(const Name &n1, const Name &n2);


std::string
Name::getCompFromBackAsString(int index) const
{
  return getCompAsString (m_comps.size () - 1 - index);
}

uint64_t
Name::getCompFromBackAsInt (int index) const
{
  return getCompAsInt (m_comps.size () - 1 - index);
}

Name
Name::getPartialNameFromBack(int start, int n/* = -1*/) const
{
  return getPartialName (m_comps.size () - 1 - start, n);
}

const Bytes &
Name::getCompFromBack (int index) const
{
  return getComp (m_comps.size () - 1 - index);
}


} // Ccnx
#endif
