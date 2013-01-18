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

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <config.h>
#include <Ice/Ice.h>
#include <chronoshare-client.ice.h>
#include <sys/stat.h>
#include <hash-helper.h>


using namespace std;
using namespace boost;
using namespace ChronoshareClient;
namespace fs = boost::filesystem;

void
usage ()
{
  cerr << "Usage: chronoshare <cmd> [<options>]\n"
       << "\n"
       << "   <cmd> is one of:\n"
       << "       version\n"
       << "       update <filename>\n"
       << "       delete <filename>\n"
       << "       move <filename> <filename>\n";
  exit (1);
}


int
main (int argc, char **argv)
{
  if (argc < 2)
    {
      usage ();
    }

  string cmd = argv[1];
  algorithm::to_lower (cmd);
  
  if (cmd == "version")
    {
      cout << "chronoshare version " << CHRONOSHARE_VERSION << endl;
      exit (0);
    }
  
  int status = 0;
  Ice::CommunicatorPtr ic;
  try
    {
      // Create a communicator
      //
      ic = Ice::initialize (argc, argv);
    
      Ice::ObjectPrx base = ic->stringToProxy("NotifyDaemon:default -p 55436");
      if (!base)
        {
          throw "Could not create proxy";
        }

      NotifyPrx notify = NotifyPrx::checkedCast (base);
      if (notify)
        {

          if (cmd == "update")
            {
              if (argc != 3)
                {
                  usage ();
                }

              fs::path file (argv[2]);
              fs::file_status fileStatus = fs::status (file);
              if (is_regular_file (fileStatus))
                {
                  // Alex: the following code is platform specific :(
                  HashPtr fileHash = Hash::FromFileContent (file);
                  
                  notify->updateFile (file.generic_string (),
                                      make_pair(reinterpret_cast<const ::Ice::Byte*> (fileHash->GetHash ()),
                                                reinterpret_cast<const ::Ice::Byte*> (fileHash->GetHash ()) +
                                                fileHash->GetHashBytes ()),
                                      fs::last_write_time (file),
                                      // fileStats.st_atime, fileStats.st_mtime, fileStats.st_ctime,
                                      fileStatus.permissions ());
                }
              else
                {
                  cerr << "File [" << argv[2] << "] doesn't exist or is not accessible" << endl;
                  return 1;
                }
            }
          else if (cmd == "delete")
            {
              if (argc != 3)
                {
                  usage ();
                }
              
              notify->deleteFile (argv[2]);
            }
          else if (cmd == "move")
            {
              if (argc != 4)
                {
                  usage ();
                }

              fs::path srcFile (argv[2]);
              fs::path dstFile (argv[3]);
              
              notify->moveFile (srcFile.generic_string (), dstFile.generic_string ());
            }
          else
            {
              cerr << "ERROR: Unknown command " << cmd << endl;
              usage ();
            }
        }
      else
        {
          cerr << "ERROR: Cannot connect to the daemon\n";
          status = 1;
        }
    }
  catch (const Ice::Exception& ex)
    {
      cerr << ex << endl;
      status = 1;
    }
  
  if (ic)
    ic->destroy();
  return status;
}
