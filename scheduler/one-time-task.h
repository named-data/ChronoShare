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

#ifndef ONE_TIME_TASK_H
#define ONE_TIME_TASK_H

#include "task.h"

class OneTimeTask : public Task
{
public:
  OneTimeTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, double delay);
  virtual ~OneTimeTask(){}

  // invoke callback and mark self as invoked and deregister self from scheduler
  virtual void
  run() _OVERRIDE;

  // after reset, the task is marked as un-invoked and can be add to scheduler again, with same delay
  // if not invoked yet, no effect
  virtual void
  reset() _OVERRIDE;

private:
  // this is to deregister itself from scheduler automatically after invoke
  void
  deregisterSelf();
};


#endif // EVENT_SCHEDULER_H
