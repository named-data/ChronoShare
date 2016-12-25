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

#ifndef CHRONOSHARE_CORE_RANDOM_INTERVAL_GENERATOR_HPP
#define CHRONOSHARE_CORE_RANDOM_INTERVAL_GENERATOR_HPP

#include "interval-generator.hpp"

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

namespace ndn {
namespace chronoshare {

// generates intervals with uniform distribution
class RandomIntervalGenerator : public IntervalGenerator
{
public:
  enum class Direction {
    UP = 1,
    DOWN = 2,
    EVEN = 3
  };

public:
  // percent is random-range/interval; e.g. if interval is 10 and you wish the random-range to be 2
  // e.g. 9 ~ 11, percent = 0.2
  // direction shifts the random range; e.g. in the above example, UP would produce a range of
  // 10 ~ 12, DOWN of 8 ~ 10, and EVEN of 9 ~ 11
  RandomIntervalGenerator(double interval, double percent, Direction direction = Direction::EVEN)
    // : m_rng(time(NULL))
    : m_rng(static_cast<int>(
        boost::posix_time::microsec_clock::local_time().time_of_day().total_nanoseconds())),
      m_dist(0.0, fractional(percent)),
      m_random(m_rng, m_dist),
      m_direction(direction),
      m_percent(percent),
      m_interval(interval)
  {
  }

  virtual ~RandomIntervalGenerator()
  {
  }

  virtual double
  nextInterval()
  {
    double percent = m_random();
    double interval = m_interval;
    switch (m_direction) {
      case Direction::UP:
        interval = m_interval * (1.0 + percent);
        break;
      case Direction::DOWN:
        interval = m_interval * (1.0 - percent);
        break;
      case Direction::EVEN:
        interval = m_interval * (1.0 - m_percent / 2.0 + percent);
        break;
      default:
        break;
    }

    return interval;
  }

private:
  inline double
  fractional(double x)
  {
    double dummy;
    return std::abs<double>(modf(x, &dummy));
  }

private:
  typedef boost::mt19937 RNG_TYPE;
  RNG_TYPE m_rng;
  boost::uniform_real<> m_dist;
  boost::variate_generator<RNG_TYPE&, boost::uniform_real<>> m_random;
  Direction m_direction;
  double m_percent;
  double m_interval;
};

} // chronoshare
} // ndn

#endif // CHRONOSHARE_CORE_RANDOM_INTERVAL_GENERATOR_HPP
