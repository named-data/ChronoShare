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
  BOOST_CHECK_EQUAL(name, msg);
}

Closure::TimeoutCallbackReturnValue timeout(const Name &name)
{
  cout << "Timeout: "<< name;
  return Closure::RESULT_OK;
}

BOOST_AUTO_TEST_CASE (CcnxWrapperTest)
{
  Name prefix1("/c1");
  Name prefix2("/c2");

  c1->setInterestFilter(prefix1, bind(publish1, _1));
  c2->setInterestFilter(prefix2, bind(publish2, _1));

  Closure *closure = new Closure(1, bind(dataCallback, _1, _2), bind(timeout, _1));

  c1->sendInterest(Name("/c2/hi"), closure);
  usleep(100000);
  c2->sendInterest(Name("/c1/hi"), closure);
  sleep(1);
  delete closure;
}

BOOST_AUTO_TEST_SUITE_END()
