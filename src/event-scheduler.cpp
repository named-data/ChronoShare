#include "event-scheduler.h"
#include <utility>

#define EVLOOP_NO_EXIT_ON_EMPTY 0x04

IntervalGeneratorPtr
IntervalGenerator:: Null;

void
eventCallback(evutil_socket_t fd, short what, void *arg)
{
  Task *task = static_cast<Task *>(arg);
  task->run();
  task = NULL;
}

Task::Task(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler)
     : m_callback(callback)
     , m_tag(tag)
     , m_scheduler(scheduler)
     , m_invoked(false)
     , m_event(NULL)
     , m_tv(NULL)
{
  m_event = evtimer_new(scheduler->base(), eventCallback, this);
  m_tv = new timeval;
}

Task::~Task()
{
  if (m_event != NULL)
  {
    event_free(m_event);
    m_event = NULL;
  }

  if (m_tv != NULL)
  {
    delete m_tv;
    m_tv = NULL;
  }
}

void
Task::setTv(double delay)
{
  double intPart, fraction;
  fraction = modf(abs(delay), &intPart);
  m_tv->tv_sec = static_cast<int>(intPart);
  m_tv->tv_usec = static_cast<int>((fraction * 1000000));
}

OneTimeTask::OneTimeTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, double delay)
            : Task(callback, tag, scheduler)
{
  setTv(delay);
}

void
OneTimeTask::run()
{
  if (!m_invoked)
  {
    m_callback();
    m_invoked = true;
    deregisterSelf();
  }
}

void
OneTimeTask::deregisterSelf()
{
  m_scheduler->deleteTask(m_tag);
}

void
OneTimeTask::reset()
{
  m_invoked = false;
}

PeriodicTask::PeriodicTask(const Callback &callback, const Tag &tag, const SchedulerPtr &scheduler, const IntervalGeneratorPtr &generator)
             : Task(callback, tag, scheduler)
             , m_generator(generator)
{
}

void
PeriodicTask::run()
{
  if (!m_invoked)
  {
    m_callback();
    m_invoked = true;
    m_scheduler->rescheduleTask(m_tag);
  }
}

void
PeriodicTask::reset()
{
  m_invoked = false;
  double interval = m_generator->nextInterval();
  setTv(interval);
}

RandomIntervalGenerator::RandomIntervalGenerator(double interval, double percent, Direction direction)
                        : m_interval(interval)
                        , m_rng(time(NULL))
                        , m_percent(percent)
                        , m_dist(0.0, fractional(percent))
                        , m_random(m_rng, m_dist)
                        , m_direction(direction)
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
  default: break;
  }

  return interval;
}

Scheduler::Scheduler()
{
  evthread_use_pthreads();
  m_base = event_base_new();
}

Scheduler::~Scheduler()
{
  event_base_free(m_base);
}

void
Scheduler::eventLoop()
{
  event_base_loop(m_base, EVLOOP_NO_EXIT_ON_EMPTY);
}

void
Scheduler::start()
{
  m_thread = boost::thread(&Scheduler::eventLoop, this);
}

void
Scheduler::shutdown()
{
  event_base_loopbreak(m_base);
  m_thread.join();
}

bool
Scheduler::addTask(const TaskPtr &task)
{
  TaskPtr newTask = task;

  if (addToMap(newTask))
  {
    newTask->reset();
    evtimer_add(newTask->ev(), newTask->tv());
    return true;
  }

  return false;
}

void
Scheduler::rescheduleTask(const Task::Tag &tag)
{
  ReadLock(m_mutex);
  TaskMapIt it = m_taskMap.find(tag);
  if (it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    task->reset();
    evtimer_add(task->ev(), task->tv());
  }
}

bool
Scheduler::addToMap(const TaskPtr &task)
{
  WriteLock(m_mutex);
  if (m_taskMap.find(task->tag()) == m_taskMap.end())
  {
    m_taskMap.insert(make_pair(task->tag(), task));
    return true;
  }
  return false;
}

void
Scheduler::deleteTask(const Task::Tag &tag)
{
  WriteLock(m_mutex);
  TaskMapIt it = m_taskMap.find(tag);
  if (it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    evtimer_del(task->ev());
    m_taskMap.erase(it);
  }
}

void
Scheduler::deleteTask(const Task::TaskMatcher &matcher)
{
  WriteLock(m_mutex);
  TaskMapIt it = m_taskMap.begin();
  while(it != m_taskMap.end())
  {
    TaskPtr task = it->second;
    if (matcher(task))
    {
      evtimer_del(task->ev());
      // Use post increment; map.erase invalidate the iterator that is beening erased,
      // but does not invalidate other iterators. This seems to be the convention to
      // erase something from C++ STL map while traversing.
      m_taskMap.erase(it++);
    }
    else
    {
      ++it;
    }
  }
}

int
Scheduler::size()
{
  ReadLock(m_mutex);
  return m_taskMap.size();
}
