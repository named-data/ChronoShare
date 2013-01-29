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

void
Closure::runTimeoutCallback(Name interest, const Closure &closure, Selectors selectors)
{
  if (!m_timeoutCallback.empty ())
    {
      m_timeoutCallback (interest, closure, selectors);
    }
}


void
Closure::runDataCallback(Name name, PcoPtr content)
{
  if (!m_dataCallback.empty ())
    {
      m_dataCallback (name, content);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

// ExecutorClosure::ExecutorClosure(const Closure &closure, ExecutorPtr executor)
//   : Closure(closure.m_dataCallback, closure.m_timeoutCallback)
//   , m_executor(executor)
// {
// }

// ExecutorClosure::~ExecutorClosure()
// {
// }

// void
// ExecutorClosure::runDataCallback(Name name, PcoPtr content)
// {
//   m_executor->execute(boost::bind(&Closure::runDataCallback, this, name, content));
// }

// // void
// // ExecutorClosure::execute(Name name, PcoPtr content)
// // {
// //   Closure::runDataCallback(name, content);
// // }


////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////


// ExecutorInterestClosure::ExecutorInterestClosure(const InterestCallback &callback, ExecutorPtr executor)
//   : m_callback(callback)
//   , m_executor(executor)
// {
// }

// void
// ExecutorInterestClosure::runInterestCallback(Name interest)
// {
//   m_executor->execute(boost::bind(&ExecutorInterestClosure::execute, this, interest));
// }

// void
// ExecutorInterestClosure::execute(Name interest)
// {
//   if (!m_callback.empty())
//   {
//     m_callback(interest);
//   }
// }

} // Ccnx
