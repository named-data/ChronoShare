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

};

DummyTunnel::DummyTunnel() : CcnxTunnel() {m_localPrefix = Name("/local");}

Name
DummyTunnel::queryRoutableName (const Name &name)
{
  return Name("/local") + name;
}

CcnxWrapperPtr t1(new DummyTunnel());
CcnxWrapperPtr t2(new DummyTunnel());
CcnxWrapperPtr c1(new CcnxWrapper());

// global data callback counter;
int g_dc_i = 0;
int g_dc_o = 0;

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


BOOST_AUTO_TEST_SUITE_END()
