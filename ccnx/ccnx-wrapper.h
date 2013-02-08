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

#ifndef CCNX_WRAPPER_H
#define CCNX_WRAPPER_H

#include <boost/thread/locks.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/thread.hpp>

#include "ccnx-common.h"
#include "ccnx-name.h"
#include "ccnx-selectors.h"
#include "ccnx-closure.h"
#include "ccnx-pco.h"
#include "executor.h"

namespace Ccnx {

struct CcnxOperationException : virtual boost::exception, virtual std::exception { };

class CcnxWrapper
{
public:
  const static int MAX_FRESHNESS = 2147; // max value for ccnx
  const static int DEFAULT_FRESHNESS = 60;
  typedef boost::function<void (Name, Selectors)> InterestCallback;

  CcnxWrapper();
  virtual ~CcnxWrapper();

  void
  start (); // called automatically in constructor

  /**
   * @brief Because of uncertainty with executor, in some case it is necessary to call shutdown explicitly (see test-server-and-fetch.cc)
   */
  void
  shutdown (); // called in destructor, but can called manually

  virtual int
  setInterestFilter (const Name &prefix, const InterestCallback &interestCallback);

  virtual void
  clearInterestFilter (const Name &prefix);

  virtual int
  sendInterest (const Name &interest, const Closure &closure, const Selectors &selector = Selectors());

  virtual int
  publishData (const Name &name, const unsigned char *buf, size_t len, int freshness = DEFAULT_FRESHNESS/* max value for ccnx*/);

  int
  publishData (const Name &name, const Bytes &content, int freshness = DEFAULT_FRESHNESS/* max value for ccnx*/);

  int
  publishUnsignedData(const Name &name, const Bytes &content, int freshness = DEFAULT_FRESHNESS);

  int
  publishUnsignedData(const Name &name, const unsigned char *buf, size_t len, int freshness = DEFAULT_FRESHNESS);

  static Name
  getLocalPrefix ();

  Bytes
  createContentObject(const Name &name, const void *buf, size_t len, int freshness = DEFAULT_FRESHNESS/* max value for ccnx*/);

  int
  putToCcnd (const Bytes &contentObject);

private:
  CcnxWrapper(const CcnxWrapper &other) {}

protected:
  void
  connectCcnd();

  /// @cond include_hidden
  void
  ccnLoop ();

  /// @endcond

protected:
  typedef boost::shared_mutex Lock;
  typedef boost::unique_lock<Lock> WriteLock;
  typedef boost::shared_lock<Lock> ReadLock;

  typedef boost::recursive_mutex RecLock;
  typedef boost::unique_lock<RecLock> UniqueRecLock;

  ccn* m_handle;
  RecLock m_mutex;
  boost::thread m_thread;
  bool m_running;
  bool m_connected;
  std::map<Name, InterestCallback> m_registeredInterests;
  ExecutorPtr m_executor;
  ccn_keystore *m_keystore;
};

typedef boost::shared_ptr<CcnxWrapper> CcnxWrapperPtr;


} // Ccnx

#endif
