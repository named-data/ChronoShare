#include "ccnx-wrapper.h"
#include "ccnx-closure.h"
#include "ccnx-name.h"
#include "ccnx-pco.h"
#include <unistd.h>

#include <boost/test/unit_test.hpp>


using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(CcnxWrapperTests)

CcnxWrapperPtr c1(new CcnxWrapper());
CcnxWrapperPtr c2(new CcnxWrapper());
int g_timeout_counter = 0;
int g_dataCallback_counter = 0;

void publish1(const Name &name)
{
  string content = name.toString();
  c1->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void publish2(const Name &name)
{
  string content = name.toString();
  c2->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void dataCallback(const Name &name, const Bytes &content)
{
  string msg((const char*)&content[0], content.size());
  g_dataCallback_counter ++;
  BOOST_CHECK_EQUAL(name, msg);
}

Closure::TimeoutCallbackReturnValue timeout(const Name &name)
{
  g_timeout_counter ++;
  return Closure::RESULT_OK;
}

BOOST_AUTO_TEST_CASE (CcnxWrapperTest)
{
  Name prefix1("/c1");
  Name prefix2("/c2");

  c1->setInterestFilter(prefix1, bind(publish1, _1));
  usleep(100000);
  c2->setInterestFilter(prefix2, bind(publish2, _1));

  Closure *closure = new Closure(1, bind(dataCallback, _1, _2), bind(timeout, _1));

  c1->sendInterest(Name("/c2/hi"), closure);
  usleep(100000);
  c2->sendInterest(Name("/c1/hi"), closure);
  sleep(1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 2);
  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
  delete closure;
}

BOOST_AUTO_TEST_CASE (CcnxWrapperSelector)
{

  Closure *closure = new Closure(1, bind(dataCallback, _1, _2), bind(timeout, _1));

  Selectors selectors;
  selectors.interestLifetime(1);

  string n1 = "/random/01";
  c1->sendInterest(Name(n1), closure, selectors);
  sleep(2);
  c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 4);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 0);

  string n2 = "/random/02";
  selectors.interestLifetime(2);
  c1->sendInterest(Name(n2), closure, selectors);
  sleep(1);
  c2->publishData(Name(n2), (const unsigned char *)n2.c_str(), n2.size(), 4);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);

  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
  delete closure;
}

BOOST_AUTO_TEST_SUITE_END()
