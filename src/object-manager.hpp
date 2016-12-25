/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 University of California, Los Angeles
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
