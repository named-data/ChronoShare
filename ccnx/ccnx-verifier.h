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

#ifndef CCNX_VERIFIER_H
#define CCNX_VERIFIER_H

#include "ccnx-common.h"
#include "ccnx-wrapper.h"
#include "ccnx-name.h"
#include "ccnx-cert.h"
#include "ccnx-pco.h"
#include <map>

namespace Ccnx {

class CcnxWrapper;

// not thread-safe, don't want to add a mutex for CertCache
// which increases the possibility of dead-locking
// ccnx-wrapper would take care of thread-safety issue
class Verifier
{
public:
  Verifier(CcnxWrapper *ccnx);
  ~Verifier();

  bool verify(const PcoPtr &pco);

private:

private:
  CcnxWrapper *m_ccnx;
  Hash m_rootKeyDigest;
  typedef std::map<Hash, CertPtr> CertCache;
  CertCache m_certCache;
};

} // Ccnx

#endif // CCNX_VERIFIER_H
