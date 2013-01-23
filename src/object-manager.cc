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
#include "ccnx-pco.h"
#include "object-db.h"
#include "logging.h"

#include <sys/stat.h>

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem/fstream.hpp>

INIT_LOGGER ("Object.Manager");

using namespace Ccnx;
using namespace boost;
using namespace std;
namespace fs = boost::filesystem;

const int MAX_FILE_SEGMENT_SIZE = 1024;

ObjectManager::ObjectManager (Ccnx::CcnxWrapperPtr ccnx, const fs::path &folder)
  : m_ccnx (ccnx)
  , m_folder (folder / ".chronoshare")
{
  fs::create_directories (m_folder);
}

ObjectManager::~ObjectManager ()
{
}

boost::tuple<HashPtr /*object-db name*/, size_t /* number of segments*/>
ObjectManager::localFileToObjects (const fs::path &file, const Ccnx::Name &deviceName)
{
  HashPtr fileHash = Hash::FromFileContent (file);
  ObjectDb fileDb (m_folder, lexical_cast<string> (*fileHash));

  fs::ifstream iff (file, std::ios::in | std::ios::binary);
  sqlite3_int64 segment = 0;
  while (iff.good ())
    {
      char buf[MAX_FILE_SEGMENT_SIZE];
      iff.read (buf, MAX_FILE_SEGMENT_SIZE);

      Name name = Name (deviceName)("file")(fileHash->GetHash (), fileHash->GetHashBytes ())(segment);

      // cout << *fileHash << endl;
      // cout << name << endl;

      Bytes data = m_ccnx->createContentObject (name, buf, iff.gcount ());
      fileDb.saveContentObject (deviceName, segment, data);

      segment ++;
    }

  return make_tuple (fileHash, segment);
}

bool
ObjectManager::objectsToLocalFile (/*in*/const Ccnx::Name &deviceName, /*in*/const Hash &fileHash, /*out*/ const fs::path &file)
{
  string hashStr = lexical_cast<string> (fileHash);
  if (!ObjectDb::DoesExist (m_folder, deviceName, hashStr))
    {
      _LOG_ERROR ("ObjectDb for [" << m_folder << ", " << deviceName << ", " << hashStr << "] does not exist or not all segments are available");
      return false;
    }

  fs::ofstream off (file, std::ios::out | std::ios::binary);
  ObjectDb fileDb (m_folder, hashStr);

  sqlite3_int64 segment = 0;
  BytesPtr bytes = fileDb.fetchSegment (deviceName, 0);
  while (bytes != BytesPtr())
    {
      ParsedContentObject obj (*bytes);
      BytesPtr data = obj.contentPtr ();

      off.write (reinterpret_cast<const char*> (head(*data)), data->size());

      segment ++;
      bytes = fileDb.fetchSegment (deviceName, segment);
    }

  // permission and timestamp should be assigned somewhere else (ObjectManager has no idea about that)

  return true;
}
