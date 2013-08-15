#include "ndnx-discovery.h"
#include "simple-interval-generator.h"
#include "task.h"
#include "periodic-task.h"
#include <sstream>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

using namespace Ndnx;
using namespace std;

const string
TaggedFunction::CHAR_SET = string("abcdefghijklmnopqtrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

TaggedFunction::TaggedFunction(const Callback &callback, const string &tag)
                   : m_callback(callback)
                   , m_tag(tag)
{
}

string
TaggedFunction::GetRandomTag()
{
  //boost::random::random_device rng;
  //boost::random::uniform_int_distribution<> dist(0, CHAR_SET.size() - 1);
  ostringstream oss;
  //for (int i = 0; i < DEFAULT_TAG_SIZE; i++)
  //{
    //oss << CHAR_SET[dist(rng)];
  //}
  return oss.str();
}

void
TaggedFunction::operator()(const Name &name)
{
  if (!m_callback.empty())
  {
    m_callback(name);
  }
}

const double
NdnxDiscovery::INTERVAL = 15.0;

NdnxDiscovery *
NdnxDiscovery::instance = NULL;

boost::mutex
NdnxDiscovery::mutex;

NdnxDiscovery::NdnxDiscovery()
              : m_scheduler(new Scheduler())
              , m_localPrefix("/")
{
  m_scheduler->start();

  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  boost::bind(&NdnxDiscovery::poll, this), "Initial-Local-Prefix-Check");
  Scheduler::schedulePeriodicTask (m_scheduler,
                                   boost::make_shared<SimpleIntervalGenerator>(INTERVAL),
                                   boost::bind(&NdnxDiscovery::poll, this), "Local-Prefix-Check");
}

NdnxDiscovery::~NdnxDiscovery()
{
  m_scheduler->shutdown();
}

void
NdnxDiscovery::addCallback(const TaggedFunction &callback)
{
  m_callbacks.push_back(callback);
}

int
NdnxDiscovery::deleteCallback(const TaggedFunction &callback)
{
  List::iterator it = m_callbacks.begin();
  while (it != m_callbacks.end())
  {
    if ((*it) == callback)
    {
      it = m_callbacks.erase(it);
    }
    else
    {
      ++it;
    }
  }
  return m_callbacks.size();
}

void
NdnxDiscovery::poll()
{
  Name localPrefix = NdnxWrapper::getLocalPrefix();
  if (localPrefix != m_localPrefix)
  {
    Lock lock(mutex);
    for (List::iterator it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
    {
      (*it)(localPrefix);
    }
    m_localPrefix = localPrefix;
  }
}

void
NdnxDiscovery::registerCallback(const TaggedFunction &callback)
{
  Lock lock(mutex);
  if (instance == NULL)
  {
    instance = new NdnxDiscovery();
  }

  instance->addCallback(callback);
}

void
NdnxDiscovery::deregisterCallback(const TaggedFunction &callback)
{
  Lock lock(mutex);
  if (instance == NULL)
  {
    cerr << "NdnxDiscovery::deregisterCallback called without instance" << endl;
  }
  else
  {
    int size = instance->deleteCallback(callback);
    if (size == 0)
    {
      delete instance;
      instance = NULL;
    }
  }
}

