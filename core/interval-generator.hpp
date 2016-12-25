/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#ifndef CHRONOSHARE_CORE_INTERVAL_GENERATOR_HPP
#define CHRONOSHARE_CORE_INTERVAL_GENERATOR_HPP

#include "chronoshare-common.hpp"

namespace ndn {
namespace chronoshare {

class IntervalGenerator;
typedef shared_ptr<IntervalGenerator> IntervalGeneratorPtr;

class IntervalGenerator
{
public:
  virtual
  ~IntervalGenerator()
  {
  }

  virtual double
  nextInterval() = 0;
};

} // chronoshare
} // ndn

#endif // CHRONOSHARE_CORE_INTERVAL_GENERATOR_HPP
