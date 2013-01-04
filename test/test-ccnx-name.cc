
#include "ccnx-name.h"

#define BOOST_TEST_MAIN 1

#include <boost/test/unit_test.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(CcnxNameTests)

BOOST_AUTO_TEST_CASE (CcnxNameTest)
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

  // Charbuf related stuff will be checked in other place
}

BOOST_AUTO_TEST_CASE (SelectorsTest)
{
  Selectors s;
  BOOST_CHECK(s.isEmpty());
}

BOOST_AUTO_TEST_SUITE_END()
