/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#include "object-manager.hpp"
#include "ccnx-name.hpp"
#include "ccnx-common.hpp"
#include "ccnx-pco.hpp"
#include "object-db.hpp"
#include "logging.hpp"

#include <sys/stat.h>

#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/throw_exception.hpp>
#include <boost/filesystem/fstream.hpp>

INIT_LOGGER ("Object.Manager");

using namespace Ndnx;
using namespace boost;
using namespace std;
namespace fs = boost::filesystem;

const int MAX_FILE_SEGMENT_SIZE = 1024;

ObjectManager::ObjectManager (Ndnx::NdnxWrapperPtr ndnx, const fs::path &folder, const std::string &appName)
  : m_ndnx (ndnx)
  , m_folder (folder / ".chronoshare")
  , m_appName (appName)
{
  fs::create_directories (m_folder);
}

ObjectManager::~ObjectManager ()
{
}

// /<devicename>/<appname>/file/<hash>/<segment>
boost::tuple<HashPtr /*object-db name*/, size_t /* number of segments*/>
ObjectManager::localFileToObjects (const fs::path &file, const Ndnx::Name &deviceName)
{
  HashPtr fileHash = Hash::FromFileContent (file);
  ObjectDb fileDb (m_folder, lexical_cast<string> (*fileHash));

  fs::ifstream iff (file, std::ios::in | std::ios::binary);
  sqlite3_int64 segment = 0;
  while (iff.good () && !iff.eof ())
    {
      char buf[MAX_FILE_SEGMENT_SIZE];
      iff.read (buf, MAX_FILE_SEGMENT_SIZE);
      if (iff.gcount () == 0)
        {
          // stupid streams...
          break;
        }

      Name name = Name ("/")(deviceName)(m_appName)("file")(fileHash->GetHash (), fileHash->GetHashBytes ())(segment);

      // cout << *fileHash << endl;
      // cout << name << endl;
      //_LOG_DEBUG ("Read " << iff.gcount () << " from " << file << " for segment " << segment);

      Bytes data = m_ndnx->createContentObject (name, buf, iff.gcount ());
      fileDb.saveContentObject (deviceName, segment, data);

      segment ++;
    }
  if (segment == 0) // handle empty files
    {
      Name name = Name ("/")(m_appName)("file")(fileHash->GetHash (), fileHash->GetHashBytes ())(deviceName)(0);
      Bytes data = m_ndnx->createContentObject (name, 0, 0);
      fileDb.saveContentObject (deviceName, 0, data);

      segment ++;
    }

  return make_tuple (fileHash, segment);
}

bool
ObjectManager::objectsToLocalFile (/*in*/const Ndnx::Name &deviceName, /*in*/const Hash &fileHash, /*out*/ const fs::path &file)
{
  string hashStr = lexical_cast<string> (fileHash);
  if (!ObjectDb::DoesExist (m_folder, deviceName, hashStr))
    {
      _LOG_ERROR ("ObjectDb for [" << m_folder << ", " << deviceName << ", " << hashStr << "] does not exist or not all segments are available");
      return false;
    }

  if (!exists (file.parent_path ()))
    {
      create_directories (file.parent_path ());
    }

  fs::ofstream off (file, std::ios::out | std::ios::binary);
  ObjectDb fileDb (m_folder, hashStr);

  sqlite3_int64 segment = 0;
  BytesPtr bytes = fileDb.fetchSegment (deviceName, 0);
  while (bytes)
    {
      ParsedContentObject obj (*bytes);
      BytesPtr data = obj.contentPtr ();

      if (data)
        {
          off.write (reinterpret_cast<const char*> (head(*data)), data->size());
        }

      segment ++;
      bytes = fileDb.fetchSegment (deviceName, segment);
    }

  // permission and timestamp should be assigned somewhere else (ObjectManager has no idea about that)

  return true;
}
