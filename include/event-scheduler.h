#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

// use pthread

#include <event2/event.h>
#include <event2/thread.h>
#include <event2/event-config.h>
#include <event2/util.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/exception/all.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <math.h>
#include <map>
#include <sys/time.h>

#define _OVERRIDE
#ifdef __GNUC__
#if __GNUC_MAJOR >= 4 && __GNUC_MINOR__ >= 7
  #undef _OVERRIDE
  #define _OVERRIDE override
#endif // __GNUC__ version
#endif // __GNUC__

using namespace std;

// callback used by libevent
static void
eventCallback(evutil_socket_t fd, short what, void *arg);

class Scheduler;
typedef boost::shared_ptr<Scheduler> SchedulerPtr;
class IntervalGenerator;
typedef boost::shared_ptr<IntervalGenerator> IntervalGeneratorPtr;
class Task;
typedef boost::shared_ptr<Task> TaskPtr;

class IntervalGenerator
{
public:
  virtual double
  nextInterval() = 0;
  static IntervalGeneratorPtr Null;
};


class Task
{
public:
  // callback of this task
  typedef boost::function<void ()> Callback;
  // tag identifies this task, should be unique
  typedef string Tag;
  // used to match tasks
  typedef boost::function<bool (const TaskPtr &task)> TaskMatcher;


  // Task is associated with Schedulers due to the requirement that libevent event is associated with an libevent event_base
  Task(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler);
  virtual ~Task();

  virtual void
  run() = 0;

  Tag
  tag() { return m_tag; }

  event *
  ev() { return m_event; }

  timeval *
  tv() { return m_tv; }

  // Task needs to be resetted after the callback is invoked if it is to be schedule again; just for safety
  // it's called by scheduler automatically when addTask or rescheduleTask is called;
  // Tasks should do preparation work here (e.g. set up new delay, etc. )
  virtual void
  reset() = 0;

  // set delay
  // This overrides whatever delay kept in m_tv
  void
  setTv(double delay);

protected:
  Callback m_callback;
  Tag m_tag;
  SchedulerPtr m_scheduler;
  bool m_invoked;
  event *m_event;
  timeval *m_tv;
};

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

struct SchedulerException : virtual boost::exception, virtual exception { };

class Scheduler
{
public:
  Scheduler();
  virtual ~Scheduler();

  // start event scheduling
  virtual void
  start();

  // stop event scheduling
  virtual void
  shutdown();

  // if task with the same tag exists, the task is not added and return false
  virtual bool
  addTask(const TaskPtr &task);

  // delete task by tag, regardless of whether it's invoked or not
  // if no task is found, no effect
  virtual void
  deleteTask(const Task::Tag &tag);

  // delete tasks by matcher, regardless of whether it's invoked or not
  // this is flexiable in that you can use any form of criteria in finding tasks to delete
  // but keep in mind this is a linear scan

  // if no task is found, no effect
  virtual void
  deleteTask(const Task::TaskMatcher &matcher);

  // task must already have been added to the scheduler, otherwise this is no effect
  // this is usually used by PeriodicTask
  virtual void
  rescheduleTask(const Task::Tag &tag);

  void
  eventLoop();

  event_base *
  base() { return m_base; }

  // used in test
  int
  size();

protected:
  bool
  addToMap(const TaskPtr &task);

protected:
  typedef map<Task::Tag, TaskPtr> TaskMap;
  typedef map<Task::Tag, TaskPtr>::iterator TaskMapIt;
  typedef boost::shared_mutex Mutex;
  typedef boost::unique_lock<Mutex> WriteLock;
  typedef boost::shared_lock<Mutex> ReadLock;
  TaskMap m_taskMap;
  Mutex m_mutex;
  bool m_running;
  event_base *m_base;
  boost::thread m_thread;
};


class SimpleIntervalGenerator : public IntervalGenerator
{
public:
  SimpleIntervalGenerator(double interval) : m_interval(interval) {}
  ~SimpleIntervalGenerator(){}
  virtual double
  nextInterval() _OVERRIDE { return m_interval; }
private:
  double m_interval;
};

// generates intervals with uniform distribution
class RandomIntervalGenerator : public IntervalGenerator
{
public:
  typedef enum
  {
    UP = 1,
    DOWN = 2,
    EVEN = 3
  } Direction;

public:
  // percent is random-range/interval; e.g. if interval is 10 and you wish the random-range to be 2
  // e.g. 9 ~ 11, percent = 0.2
  // direction shifts the random range; e.g. in the above example, UP would produce a range of
  // 10 ~ 12, DOWN of 8 ~ 10, and EVEN of 9 ~ 11
  RandomIntervalGenerator(double interval, double percent, Direction direction = EVEN);
  ~RandomIntervalGenerator(){}
  virtual double
  nextInterval() _OVERRIDE;

private:
  inline double fractional(double x) { double dummy; return abs(modf(x, &dummy)); }

private:
  typedef boost::mt19937 RNG_TYPE;
  RNG_TYPE m_rng;
  boost::uniform_real<> m_dist;
  boost::variate_generator<RNG_TYPE &, boost::uniform_real<> > m_random;
  Direction m_direction;
  double m_interval;
  double m_percent;

};
#endif // EVENT_SCHEDULER_H
