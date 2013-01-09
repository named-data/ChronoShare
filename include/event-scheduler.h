#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H

// use pthread

#include <event2/event.h>
#include <event2/thread.h>

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
  typedef boost::function<void ()> Callback;
  typedef string Tag;
  typedef boost::function<bool (const TaskPtr &task)> TaskMatcher;

  Task(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, const IntervalGeneratorPtr &generator = IntervalGenerator::Null);
  ~Task();

  virtual void
  run();

  Tag
  tag() { return m_tag; }

  event *
  ev() { return m_event; }

  timeval *
  tv() { return m_tv; }

  void
  setTv(double delay);

  bool
  isPeriodic() { return m_generator != IntervalGenerator::Null; }

  void
  reset();

protected:
  void
  selfClean();

protected:
  Callback m_callback;
  Tag m_tag;
  SchedulerPtr m_scheduler;
  bool m_invoked;
  event *m_event;
  timeval *m_tv;
  IntervalGeneratorPtr m_generator;
};


struct SchedulerException : virtual boost::exception, virtual exception { };

class Scheduler
{
public:
  Scheduler();
  virtual ~Scheduler();

  virtual void
  start();

  virtual void
  shutdown();

  virtual bool
  addTask(const TaskPtr &task, double delay);

  virtual bool
  addTask(const TaskPtr &task);

  virtual void
  deleteTask(const Task::Tag &tag);

  virtual void
  deleteTask(const Task::TaskMatcher &matcher);

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
