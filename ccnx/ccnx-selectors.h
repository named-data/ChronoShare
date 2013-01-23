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
#ifndef CCNX_SELECTORS_H
#define CCNX_SELECTORS_H

#include "ccnx-common.h"
#include "ccnx-name.h"

namespace Ccnx {

struct InterestSelectorException:
    virtual boost::exception, virtual std::exception {};

class Selectors
{
public:
  Selectors();
  Selectors(const Selectors &other);
  ~Selectors(){};

  typedef enum
  {
    AOK_CS = 0x1,
    AOK_NEW = 0x2,
    AOK_DEFAULT = 0x3, // (AOK_CS | AOK_NEW)
    AOK_STALE = 0x4,
    AOK_EXPIRE = 0x10
  } AOK;

  typedef enum
  {
    LEFT = 0,
    RIGHT = 1,
    DEFAULT = 2
  } CHILD_SELECTOR;

  inline Selectors &
  maxSuffixComps(int maxSCs) {m_maxSuffixComps = maxSCs; return *this;}

  inline Selectors &
  minSuffixComps(int minSCs) {m_minSuffixComps = minSCs; return *this;}

  inline Selectors &
  answerOriginKind(AOK kind) {m_answerOriginKind = kind; return *this;}

  inline Selectors &
  interestLifetime(double lifetime) {m_interestLifetime = lifetime; return *this;}

  inline Selectors &
  scope(int scope) {m_scope = scope; return *this;}

  inline Selectors &
  childSelector(CHILD_SELECTOR child) {m_childSelector = child; return *this;}

  // this has no effect now
  inline Selectors &
  publisherPublicKeyDigest(const Bytes &digest) {m_publisherPublicKeyDigest = digest; return *this;}

  CcnxCharbufPtr
  toCcnxCharbuf() const;

  bool
  isEmpty() const;

  bool
  operator==(const Selectors &other);

private:
  int m_maxSuffixComps;
  int m_minSuffixComps;
  AOK m_answerOriginKind;
  double m_interestLifetime;
  int m_scope;
  CHILD_SELECTOR m_childSelector;
  // not used now
  Bytes m_publisherPublicKeyDigest;
};

} // Ccnx

#endif
