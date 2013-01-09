#include "event-scheduler.h"

#include <boost/test/unit_test.hpp>
#include <map>
#include <unistd.h>

using namespace boost;
using namespace std;

BOOST_AUTO_TEST_SUITE(SchedulerTests)

map<string, int> table;

void func(string str)
{
  map<string, int>::iterator it = table.find(str);
  if (it == table.end())
  {
    table.insert(make_pair(str, 1));
  }
  else
  {
    int count = it->second;
    count++;
    table.erase(it);
    table.insert(make_pair(str, count));
  }
}

bool
matcher(const TaskPtr &task)
{
  return task->tag() == "period" || task->tag() == "world";
}

BOOST_AUTO_TEST_CASE(SchedulerTest)
{
  SchedulerPtr scheduler(new Scheduler());
  IntervalGeneratorPtr generator(new SimpleIntervalGenerator(0.2));

  string tag1 = "hello";
  string tag2 = "world";
  string tag3 = "period";

  TaskPtr task1(new Task(boost::bind(func, tag1), tag1, scheduler));
  TaskPtr task2(new Task(boost::bind(func, tag2), tag2, scheduler));
  TaskPtr task3(new Task(boost::bind(func, tag3), tag3, scheduler, generator));

  scheduler->start();
  scheduler->addTask(task1, 0.5);
  scheduler->addTask(task2, 0.5);
  scheduler->addTask(task3);
  BOOST_CHECK_EQUAL(scheduler->size(), 3);
  usleep(600000);
  BOOST_CHECK_EQUAL(scheduler->size(), 1);
  task1->reset();
  scheduler->addTask(task1, 0.5);
  BOOST_CHECK_EQUAL(scheduler->size(), 2);
  usleep(600000);
  task1->reset();
  scheduler->addTask(task1, 0.5);
  BOOST_CHECK_EQUAL(scheduler->size(), 2);
  usleep(400000);
  scheduler->deleteTask(task1->tag());
  BOOST_CHECK_EQUAL(scheduler->size(), 1);
  usleep(200000);

  task1->reset();
  task2->reset();
  scheduler->addTask(task1, 0.5);
  scheduler->addTask(task2, 0.5);
  BOOST_CHECK_EQUAL(scheduler->size(), 3);
  usleep(100000);
  scheduler->deleteTask(bind(matcher, _1));
  BOOST_CHECK_EQUAL(scheduler->size(), 1);
  usleep(1000000);

  scheduler->shutdown();

  int hello = 0, world = 0, period = 0;

  map<string, int>::iterator it;
  it = table.find(tag1);
  if (it != table.end())
  {
    hello = it->second;
  }
  it = table.find(tag2);
  if (it != table.end())
  {
    world = it->second;
  }
  it = table.find(tag3);
  if (it != table.end())
  {
    period = it->second;
  }

  // added four times, canceled once before invoking callback
  BOOST_CHECK_EQUAL(hello, 3);
  // added two times, canceled once by matcher before invoking callback
  BOOST_CHECK_EQUAL(world, 1);
  // invoked every 0.2 seconds before deleted by matcher
  BOOST_CHECK_EQUAL(period, static_cast<int>((0.6 + 0.6 + 0.4 + 0.2 + 0.1) / 0.2));

}

BOOST_AUTO_TEST_CASE(GeneratorTest)
{
  double interval = 10;
  double percent = 0.5;
  int times = 10000;
  IntervalGeneratorPtr generator(new RandomIntervalGenerator(interval, percent));
  double sum = 0.0;
  double min = 2 * interval;
  double max = -1;
  for (int i = 0; i < times; i++)
  {
    double next = generator->nextInterval();
    sum += next;
    if (next > max)
    {
      max = next;
    }
    if (next < min)
    {
      min = next;
    }
  }

  BOOST_CHECK( abs(1.0 - (sum / static_cast<double>(times)) / interval) < 0.05);
  BOOST_CHECK( min > interval * (1 - percent / 2.0));
  BOOST_CHECK( max < interval * (1 + percent / 2.0));
  BOOST_CHECK( abs(1.0 - ((max - min) / interval) / percent) < 0.05);

}

BOOST_AUTO_TEST_SUITE_END()