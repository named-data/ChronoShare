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
#include "ccnx-name.h"
#include "ccnx-common.h"
#include "object-db.h"

#include <sys/stat.h>

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/throw_exception.hpp>

using namespace Ccnx;
using namespace boost;
using namespace std;
namespace fs = boost::filesystem;

const int MAX_FILE_SEGMENT_SIZE = 1024;

ObjectManager::ObjectManager (Ccnx::CcnxWrapperPtr ccnx, const Ccnx::Name &localDeviceName, const fs::path &folder)
  : m_ccnx (ccnx)
  , m_localDeviceName (localDeviceName)
  , m_folder (folder / ".chronoshare")
{
  fs::create_directories (m_folder);
}

ObjectManager::~ObjectManager ()
{
}

HashPtr
ObjectManager::storeLocalFile (const fs::path &file)
{
  HashPtr fileHash = Hash::FromFileContent (file);
  ObjectDb fileDb (m_folder, lexical_cast<string> (*fileHash));
  
  ifstream iff (file.c_str (), std::ios::in | std::ios::binary);
  int segment = 0;
  while (iff.good ())
    {
      char buf[MAX_FILE_SEGMENT_SIZE];
      iff.read (buf, MAX_FILE_SEGMENT_SIZE);


      Name name (m_localDeviceName);
      name
        .appendComp ("file")
        .appendComp (fileHash->GetHash (), fileHash->GetHashBytes ())
        .appendComp (segment);

      cout << *fileHash << endl;
      cout << name << endl;
      
      segment ++;
    }
  
  return fileHash;
}
