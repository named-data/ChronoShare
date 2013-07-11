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

#ifndef CCNX_DISCOVERY_H
#define CCNX_DISCOVERY_H

#include "ccnx-wrapper.h"
#include "ccnx-common.h"
#include "ccnx-name.h"
#include "scheduler.h"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/random.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <list>

namespace Ccnx
{

class CcnxDiscovery;
typedef boost::shared_ptr<CcnxDiscovery> CcnxDiscoveryPtr;

class TaggedFunction
{
public:
  typedef boost::function<void (const Name &)> Callback;
  TaggedFunction(const Callback &callback, const string &tag = GetRandomTag());
  ~TaggedFunction(){};

  bool
  operator==(const TaggedFunction &other) { return m_tag == other.m_tag; }

  void
  operator()(const Name &name);

private:
  static const std::string CHAR_SET;
  static const int DEFAULT_TAG_SIZE = 32;

  static std::string
  GetRandomTag();

private:
  Callback m_callback;
  std::string m_tag;
};

class CcnxDiscovery
{
public:
  const static double INTERVAL;
  // Add a callback to be invoked when local prefix changes
  // you must remember to deregister the callback
  // otherwise you may have undefined behavior if the callback is
  // bind to a member function of an object and the object is deleted
  static void
  registerCallback(const TaggedFunction &callback);

  // remember to call this before you quit
  static void
  deregisterCallback(const TaggedFunction &callback);

private:
  CcnxDiscovery();
  ~CcnxDiscovery();

  void
  poll();

  void
  addCallback(const TaggedFunction &callback);

  int
  deleteCallback(const TaggedFunction &callback);

private:
  typedef boost::mutex Mutex;
  typedef boost::unique_lock<Mutex> Lock;
  typedef std::list<TaggedFunction> List;

  static CcnxDiscovery *instance;
  static Mutex mutex;
  List m_callbacks;
  SchedulerPtr m_scheduler;
  Name m_localPrefix;
};

} // Ccnx
#endif // CCNX_DISCOVERY_H
