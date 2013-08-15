/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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
 */

#include <QtCore>

#include "dispatcher.h"
#include "fs-watcher.h"
#include "logging.h"
#include "ndnx-wrapper.h"

#include <boost/make_shared.hpp>

using namespace boost;
using namespace std;
using namespace Ndnx;

int main(int argc, char *argv[])
{
  INIT_LOGGERS ();

  QCoreApplication app(argc, argv);

  if (argc != 4)
    {
      cerr << "Usage: ./csd <username> <shared-folder> <path>" << endl;
      return 1;
    }

  string username = argv[1];
  string sharedFolder = argv[2];
  string path = argv[3];

  cout << "Starting ChronoShare for [" << username << "] shared-folder [" << sharedFolder << "] at [" << path << "]" << endl;

  Dispatcher dispatcher (username, sharedFolder, path, make_shared<NdnxWrapper> ());

  FsWatcher watcher (path.c_str (),
                     bind (&Dispatcher::Did_LocalFile_AddOrModify, &dispatcher, _1),
                     bind (&Dispatcher::Did_LocalFile_Delete,      &dispatcher, _1));

  return app.exec ();
}
