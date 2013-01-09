#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/exception/all.hpp>
#include <math.h>
#include <multimap>

#define _OVERRIDE
#ifdef __GNUC__
#if __GNUC_MAJOR >= 4 && __GNUC_MINOR__ >= 7
  #undef _OVERRIDE
  #define _OVERRIDE override
#endif // __GNUC__ version
#endif // __GNUC__

using namespace std;


class Task
{
public:
  typedef boost::function<void (void *)> Callback;
  typedef string Tag;
  typedef boost::function<bool (const Task &task)> TaskMatcher;

  Task(const Callback &callback, void *arg, const Tag &tag);
  Task(const Task &other);
  Task &
  operator=(const Task &other);

  virtual void
  run() { m_callback(m_arg); }

  Tag
  tag() { return m_tag; }


protected:
  Callback m_callback;
  Tag m_tag;
  void *arg;
};

struct SchedulerException : virtual boost::exception, virtual exception { };

class IntervalGenerator;
typedef boost::shared_ptr<IntervalGenerator> IntervalGeneratorPtr;
class Scheduler
{
public:
  Scheduler();
  virtual ~Scheduler();

  virtual void
  start();

  virtual void
  shutdown();

  virtual void
  addTask(const Task &task, double delay);

  virtual void
  addTimeoutTask(const Task &task, double timeout);

  virtual void
  addPeriodicTask(const Task &task, const IntervalGeneratorPtr &generator);

  virtual void
  deleteTask(const Tag &tag);

  virtual void
  deleteTask(const TaskMatcher &matcher);

protected:
  typedef multimap<Tag, Task> TaskMap;
  TaskMap m_taskMap;
};

class IntervalGenerator
{
public:
  virtual double
  nextInterval() = 0;
};

class SimpleIntervalGenerator : IntervalGenerator
{
public:
  SimpleIntervalGenerator(double interval) : m_interval(interval) {}
  ~SimpleIntervalGenerator(){}
  virtual double
  nextInterval() _OVERRIDE { return m_interval; }
private:
  SimpleIntervalGenerator(const SimpleIntervalGenerator &other){};
private:
  double m_interval;
};

class RandomIntervalGenerator : IntervalGenerator
{
  typedef enum
  {
    UP,
    DOWN,
    EVEN
  } Direction;

public:
  RandomIntervalGenerator(double interval, double percent, Direction direction = EVEN);
  ~RandomIntervalGenerator(){}
  virtual double
  nextInterval() _OVERRIDE;

private:
  RandomIntervalGenerator(const RandomIntervalGenerator &other){};
  inline double fractional(double x) { double dummy; return abs(modf(x, &dummy)); }

private:
  typedef boost::mt19937 RNG_TYPE;
  RNG_TYPE m_rng;
  boost::uniform_real<> m_dist;
  boost::rariate_generator<RNG_TYPE &, boost::uniform_real<> > m_random;
  Direction m_direction;
  double m_interval;
  double m_percent;

};
#endif // EVENT_SCHEDULER_H
