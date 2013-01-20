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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#ifndef FETCHER_H
#define FETCHER_H

#include "ccnx-wrapper.h"
#include "ccnx-name.h"

#include "scheduler.h"
#include <boost/intrusive/list.hpp>

class FetchManager;

class Fetcher
{
public:
  Fetcher (FetchManager &fetchManger,
           const Ccnx::Name &name, int32_t minSeqNo, int32_t maxSeqNo,
           const Ccnx::Name &forwardingHint = Ccnx::Name ());
  virtual ~Fetcher ();

private:
  void
  RestartPipeline ();

  void
  OnData (uint32_t seqno, const Ccnx::Name &name, const Ccnx::Bytes &);

  void
  OnTimeout (uint32_t seqno, const Ccnx::Name &name);

private:
  FetchManager &m_fetchManager;
  bool m_active;

  Ccnx::Name m_name;
  Ccnx::Name m_forwardingHint;

  int32_t m_minSendSeqNo;
  int32_t m_maxSendSeqNo;
  int32_t m_minSeqNo;
  int32_t m_maxSeqNo;

  uint32_t m_pipeline;

  // Ccnx::Closure m_onDataClosure;
  // Ccnx::Closure m_onTimeoutClosure;

  boost::intrusive::list_member_hook<> m_managerListHook;
  friend class FetchManager;
};

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;

namespace Error {
struct Fetcher : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<Fetcher> FetcherPtr;


#endif // FETCHER_H
