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

#ifndef SIMPLE_INTERVAL_GENERATOR_H
#define SIMPLE_INTERVAL_GENERATOR_H

#include "interval-generator.h"

class SimpleIntervalGenerator : public IntervalGenerator
{
public:
  SimpleIntervalGenerator(double interval) : m_interval (interval) {}
  virtual ~SimpleIntervalGenerator() {}
  
  virtual double
  nextInterval() _OVERRIDE { return m_interval; }

private:
  double m_interval;
};

#endif // SIMPLE_INTERVAL_GENERATOR_H
