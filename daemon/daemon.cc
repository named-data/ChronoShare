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

#include "action-log.h"
#include <iostream>
#include <Ice/Service.h>
#include <Ice/Identity.h>

#include "notify-i.h"

using namespace std;
using namespace boost;
using namespace ChronoshareClient;

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str; 

class MyService : public Ice::Service
{
protected:
  virtual bool start (int, char*[], int&)
  {
    _adapter = communicator ()->createObjectAdapterWithEndpoints ("ChronoShare", "default -p 55436");

    Ice::Identity identity;
    identity.name="NotifyDaemon";
    NotifyPtr servant=new NotifyI;
    
    _adapter->add (servant, identity);
    
    _adapter->activate();
    // status = EXIT_SUCCESS;
    return true;    
  }
  
private:
    Ice::ObjectAdapterPtr _adapter;  
};

int
main (int argc, char **argv)
{
  int status = 0;
  
  try
    {
      // DbHelper db ("./", "/ndn/ucla.edu/alex");
      ActionLog bla ("./", "/ndn/ucla.edu/alex");

      MyService svc;
      status = svc.main (argc, argv);

      // HashPtr hash = db.RememberStateInStateLog ();
      // // should be empty
      // cout << "Hash: [" << *hash << "]" << endl;

      // //
      // db.UpdateDeviceSeqno ("Alex", 1);
      // hash = db.RememberStateInStateLog ();
      // cout << "Hash: [" << *hash << "]" << endl;

      // db.UpdateDeviceSeqno ("Alex", 2);
      // hash = db.RememberStateInStateLog ();
      // cout << "Hash: [" << *hash << "]" << endl;

      // db.UpdateDeviceSeqno ("Alex", 2);
      // hash = db.RememberStateInStateLog ();
      // cout << "Hash: [" << *hash << "]" << endl;

      // db.UpdateDeviceSeqno ("Alex", 1);
      // hash = db.RememberStateInStateLog ();
      // cout << "Hash: [" << *hash << "]" << endl;

      // db.FindStateDifferences ("00", "ec0a9941fa726e1fb8f34ecdbd8e3faa75dc9dba22e6a2ea1d8482aae5fdfb52");
      // db.FindStateDifferences ("ec0a9941fa726e1fb8f34ecdbd8e3faa75dc9dba22e6a2ea1d8482aae5fdfb52", "00");
      // db.FindStateDifferences ("869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce",
      //                          "ec0a9941fa726e1fb8f34ecdbd8e3faa75dc9dba22e6a2ea1d8482aae5fdfb52");
      // db.FindStateDifferences ("ec0a9941fa726e1fb8f34ecdbd8e3faa75dc9dba22e6a2ea1d8482aae5fdfb52",
      //                          "869c38c6dffe8911ced320aecc6d9244904d13d3e8cd21081311f2129b4557ce");

      // db.UpdateDeviceSeqno ("Bob", 1);
      // hash = db.RememberStateInStateLog ();
      // cout << "Hash: [" << *hash << "]" << endl;

      // db.FindStateDifferences ("00", "48f4d95b503b9a79c2d5939fa67722b13fc01db861fc501d09efd0a38dbafab8");
      // db.FindStateDifferences ("ec0a9941fa726e1fb8f34ecdbd8e3faa75dc9dba22e6a2ea1d8482aae5fdfb52",
      //                          "48f4d95b503b9a79c2d5939fa67722b13fc01db861fc501d09efd0a38dbafab8");
    }
  catch (const Ice::Exception& e)
    {
      cerr << e << endl;
      status = 1;
    }
  catch (const boost::exception &e)
    {
      cout << "ERRORR: " << *get_error_info<errmsg_info_str> (e) << endl;
      status = 1;
    }

  return status;
}
