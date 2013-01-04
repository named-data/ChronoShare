#include <boost/test/unit_test.hpp>
#include "ccnx-name.h"
using namespace Ccnx;
using namespace std;
using namespace boost;
//#define BOOST_TEST_MAIN 1
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_Module Main

BOOST_AUTO_TEST_SUITE(CcnxNameTests)

BOOST_AUTO_TEST_CASE (CcnxNameTest)
{
  Name empty = Name();
  Name root = Name("/");
  BOOST_CHECK_EQUAL(empty, root);
}

BOOST_AUTO_TEST_CASE (SelectorsTest)
{
  int i = 0;
}

BOOST_AUTO_TEST_SUITE_END()
