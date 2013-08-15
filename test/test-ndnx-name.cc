
#include "ndnx-name.h"

#define BOOST_TEST_MAIN 1

#include <boost/test/unit_test.hpp>

using namespace Ndnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(NdnxNameTests)

BOOST_AUTO_TEST_CASE (NdnxNameTest)
{
  Name empty = Name();
  Name root = Name("/");
  BOOST_CHECK_EQUAL(empty, root);
  BOOST_CHECK_EQUAL(empty, "/");
  BOOST_CHECK_EQUAL(root.size(), 0);
  empty.appendComp("hello");
  empty.appendComp("world");
  BOOST_CHECK_EQUAL(empty.size(), 2);
  BOOST_CHECK_EQUAL(empty.toString(), "/hello/world");
  empty = empty + root;
  BOOST_CHECK_EQUAL(empty.toString(), "/hello/world");
  BOOST_CHECK_EQUAL(empty.getCompAsString(0), "hello");
  BOOST_CHECK_EQUAL(empty.getPartialName(1, 1), Name("/world"));
  Name name("/hello/world");
  BOOST_CHECK_EQUAL(empty, name);
  BOOST_CHECK_EQUAL(name, Name("/hello") + Name("/world"));


  name.appendComp (1);
  name.appendComp (255);
  name.appendComp (256);
  name.appendComp (1234567890);

  BOOST_CHECK_EQUAL (name.toString (), "/hello/world/%00%01/%00%ff/%00%00%01/%00%d2%02%96I");

  BOOST_CHECK_EQUAL (name.getCompAsInt (5), 1234567890);
  BOOST_CHECK_EQUAL (name.getCompAsInt (4), 256);
  BOOST_CHECK_EQUAL (name.getCompAsInt (3), 255);
  BOOST_CHECK_EQUAL (name.getCompAsInt (2), 1);
  
  // Charbuf related stuff will be checked in other place
}

BOOST_AUTO_TEST_SUITE_END()
