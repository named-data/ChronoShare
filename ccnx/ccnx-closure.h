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

#ifndef CCNX_CLOSURE_H
#define CCNX_CLOSURE_H

#include "ccnx-common.h"
#include "ccnx-name.h"

namespace Ccnx {

class ParsedContentObject;
typedef boost::shared_ptr<ParsedContentObject> PcoPtr;

class Closure
{
public:
  typedef boost::function<void (const Name &, PcoPtr pco)> DataCallback;

  typedef enum
  {
    RESULT_OK,
    RESULT_REEXPRESS
  } TimeoutCallbackReturnValue;

  typedef boost::function<TimeoutCallbackReturnValue (const Name &)> TimeoutCallback;

  Closure(const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback = TimeoutCallback());
  virtual ~Closure();

  virtual void
  runDataCallback(const Name &name, Ccnx::PcoPtr pco);

  virtual TimeoutCallbackReturnValue
  runTimeoutCallback(const Name &interest);

  virtual Closure *
  dup() const;

protected:
  TimeoutCallback m_timeoutCallback;
  DataCallback m_dataCallback;
};

} // Ccnx

#endif
