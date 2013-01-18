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

#ifndef PERIODIC_TASK_H
#define PERIODIC_TASK_H

#include "task.h"
#include "scheduler.h"
#include "interval-generator.h"

class PeriodicTask : public Task
{
public:
  // generator is needed only when this is a periodic task
  // two simple generators implementation (SimpleIntervalGenerator and RandomIntervalGenerator) are provided;
  // if user needs more complex pattern in the intervals between calls, extend class IntervalGenerator
  PeriodicTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, const IntervalGeneratorPtr &generator);
  virtual ~PeriodicTask(){}

  // invoke callback, reset self and ask scheduler to schedule self with the next delay interval
  virtual void
  run() _OVERRIDE;

  // set the next delay and mark as un-invoke
  virtual void
  reset() _OVERRIDE;

private:
  IntervalGeneratorPtr m_generator;
};

#endif // PERIODIC_TASK_H
