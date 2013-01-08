#include "ccnx-tunnel.h"
#include "ccnx-closure.h"
#include "ccnx-name.h"
#include "ccnx-pco.h"
#include <unistd.h>

#include <boost/test/unit_test.hpp>


using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(CcnxTunnelTests)

class DummyTunnel : public CcnxTunnel
{
public:
  DummyTunnel();
  virtual ~DummyTunnel() {}

  virtual Name
  queryRoutableName(const Name &name);

  void
  overridePrefix();

};

DummyTunnel::DummyTunnel() : CcnxTunnel()
{
  m_localPrefix = Name("/local");
}

void
DummyTunnel::overridePrefix()
{
  CcnxWrapper::setInterestFilter(m_localPrefix, bind(&DummyTunnel::handleTunneledInterest, this, _1));
}

Name
DummyTunnel::queryRoutableName (const Name &name)
{
  return Name("/local") + name;
}

CcnxWrapperPtr t1(new DummyTunnel());
CcnxWrapperPtr t2(new DummyTunnel());
CcnxWrapperPtr c1(new CcnxWrapper());

DummyTunnel t3;

// global data callback counter;
int g_dc_i = 0;
int g_dc_o = 0;
int g_ic = 0;

void innerCallback(const Name &name, const Bytes &content)
{
  g_dc_i ++;
  string str((const char *)&content[0], content.size());
  BOOST_CHECK_EQUAL(name, str);
}

void outerCallback(const Name &name, const Bytes &content)
{
  g_dc_o ++;
}

void interestCallback(const Name &name)
{
  string strName = name.toString();
  t3.publishData(name, (const unsigned char *)strName.c_str(), strName.size(), 5);
  g_ic++;
}

BOOST_AUTO_TEST_CASE (CcnxTunnelTest)
{
  // test publish
  string inner = "/hello";

  g_dc_o = 0;
  t1->publishData(Name(inner), (const unsigned char *)inner.c_str(), inner.size(), 5);
  usleep(100000);
  Closure *outerClosure = new Closure(1, bind(outerCallback, _1, _2));
  c1->sendInterest(Name("/local/hello"), outerClosure);
  usleep(100000);
  // it is indeed published as /local/hello
  BOOST_CHECK_EQUAL(g_dc_o, 1);

  g_dc_i = 0;
  Closure *innerClosure = new Closure(1, bind(innerCallback, _1, _2));
  t2->sendInterest(Name(inner), innerClosure);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_dc_i, 1);

  delete outerClosure;
  delete innerClosure;

}

BOOST_AUTO_TEST_CASE (CcnxTunnelRegister)
{

  g_ic = 0;
  g_dc_i = 0;
  t3.overridePrefix();
  t3.setInterestFilter(Name("/t3"), bind(interestCallback, _1));
  usleep(100000);
  Closure *innerClosure = new Closure(1, bind(innerCallback, _1, _2));
  t1->sendInterest(Name("/t3/hello"), innerClosure);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_dc_i, 1);
  BOOST_CHECK_EQUAL(g_ic, 1);

  t3.clearInterestFilter(Name("/t3"));
  usleep(100000);
  t1->sendInterest(Name("/t3/hello-there"), innerClosure);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_dc_i, 1);
  BOOST_CHECK_EQUAL(g_ic, 1);
  delete innerClosure;

}


BOOST_AUTO_TEST_SUITE_END()
