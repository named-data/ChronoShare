/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#include "object-manager.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <unistd.h>
#include <boost/make_shared.hpp>
#include <iostream>

using namespace Ccnx;
using namespace std;
using namespace boost;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(ObjectManagerTests)

BOOST_AUTO_TEST_CASE (ObjectManagerTest)
{
  path tmpdir = unique_path (temp_directory_path () / "%%%%-%%%%-%%%%-%%%%");
  create_directories (tmpdir);

  Name deviceName ("/device");
  
  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();
  ObjectManager manager (ccnx, deviceName, tmpdir);


  remove_all (tmpdir);
}


BOOST_AUTO_TEST_SUITE_END()
