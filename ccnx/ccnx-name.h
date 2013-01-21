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
  ~CcnxCharbuf();

  // expose internal data structure, use with caution!!
  ccn_charbuf *
  getBuf() { return m_buf; }
  static CcnxCharbufPtr Null;

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


struct NameException:
  virtual boost::exception, virtual exception {};

class Name
{
public:
  Name();
  Name(const string &name);
  Name(const vector<Bytes> &comps);
  Name(const Name &other);
  Name(const unsigned char *data, const ccn_indexbuf *comps);
  Name (const unsigned char *buf, const size_t length);
  virtual ~Name() {}

  CcnxCharbufPtr
  toCcnxCharbuf() const;

  operator CcnxCharbufPtr () const { return toCcnxCharbuf (); }

  Name &
  appendComp(const Name &comp);

  Name &
  appendComp(const Bytes &comp);

  Name &
  appendComp(const string &compStr);

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

  int
  size() const {return m_comps.size();}

  Bytes
  getComp(int index) const;

  // return string format of the comp
  // if all characters are printable, simply returns the string
  // if not, print the bytes in hex string format
  string
  getCompAsString(int index) const;

  uint64_t
  getCompAsInt (int index) const;

  Name
  getPartialName(int start, int n = -1) const;

  string
  toString() const;

  Name &
  operator=(const Name &other);

  bool
  operator==(const string &str) const;

  bool
  operator!=(const string &str) const;

  friend Name
  operator+(const Name &n1, const Name &n2);

protected:
  vector<Bytes> m_comps;
};

ostream&
operator <<(ostream &os, const Name &name);

bool
operator ==(const Name &n1, const Name &n2);

bool
operator !=(const Name &n1, const Name &n2);

bool
operator <(const Name &n1, const Name &n2);



} // Ccnx
#endif
