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

#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include <string>
#include <ndnx-wrapper.h>
#include <hash-helper.h>
#include <boost/filesystem.hpp>
#include <boost/tuple/tuple.hpp>

// everything related to managing object files

class ObjectManager
{
public:
  ObjectManager (Ndnx::NdnxWrapperPtr ndnx, const boost::filesystem::path &folder, const std::string &appName);
  virtual ~ObjectManager ();

  /**
   * @brief Creates and saves local file in a local database file
   *
   * Format: /<appname>/file/<hash>/<devicename>/<segment>
   */
  boost::tuple<HashPtr /*object-db name*/, size_t /* number of segments*/>
  localFileToObjects (const boost::filesystem::path &file, const Ndnx::Name &deviceName);

  bool
  objectsToLocalFile (/*in*/const Ndnx::Name &deviceName, /*in*/const Hash &hash, /*out*/ const boost::filesystem::path &file);

private:
  Ndnx::NdnxWrapperPtr m_ndnx;
  boost::filesystem::path m_folder;
  std::string m_appName;
};

typedef boost::shared_ptr<ObjectManager> ObjectManagerPtr;

namespace Error {
struct ObjectManager : virtual boost::exception, virtual std::exception { };
}

#endif // OBJECT_MANAGER_H
