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

#include "ccnx-closure.h"

namespace Ccnx {

Closure::Closure(const DataCallback &dataCallback, const TimeoutCallback &timeoutCallback)
  : m_timeoutCallback (timeoutCallback)
  , m_dataCallback (dataCallback)
{
}

Closure::~Closure ()
{
}

Closure::TimeoutCallbackReturnValue
Closure::runTimeoutCallback(const Name &interest)
{
  if (m_timeoutCallback.empty ())
    {
      return RESULT_OK;
    }

  return m_timeoutCallback (interest);
}


void
Closure::runDataCallback(const Name &name, const Bytes &content)
{
  if (!m_dataCallback.empty ())
    {
      m_dataCallback (name, content);
    }
}

Closure *
Closure::dup () const
{
  return new Closure (*this);
}

} // Ccnx
