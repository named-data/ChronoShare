#include "event-scheduler.h"

Task::Task(const Callback &callback, void *arg, const Tag &tag)
     : m_callback(callback)
     , m_arg(arg)
     , m_tag(tag)
{
}

Task::Task(const Task &other)
     : m_callback(other.m_callback)
     , m_arg(other.m_arg)
     , m_tag(other.m_tag)
{
}

Task &
Task::operator=(const Task &other)
{
  m_callback = other.m_callback;
  m_arg = other.m_arg;
  m_tag = other.m_tag;
  return (*this);
}

RandomIntervalGenerator::RandomIntervalGenerator(double interval, double percent, Direction direction = UP)
                        : m_interval(interval)
                        , m_rng(time(NULL))
                        , m_percent(percent)
                        , m_dist(0.0, fractional(percent))
                        , m_random(m_rng, m_dist)
{
}

double
RandomIntervalGenerator::nextInterval()
{
  double percent = m_random();
  double interval = m_interval;
  switch (m_direction)
  {
  case UP: interval = m_interval * (1.0 + percent); break;
  case DOWN: interval = m_interval * (1.0 - percent); break;
  case EVEN: interval = m_interval * (1.0 - m_percent/2.0 + percent); break;
  default: break
  }

  return interval;
}
