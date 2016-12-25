/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
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

#ifndef CHRONOSHARE_SRC_OBJECT_MANAGER_HPP
#define CHRONOSHARE_SRC_OBJECT_MANAGER_HPP

#include "core/chronoshare-common.hpp"

#include <boost/filesystem.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/digest.hpp>

// everything related to managing object files

namespace ndn {
namespace chronoshare {

class ObjectManager
{
public:
  ObjectManager(Face& face, KeyChain& keyChain,
                const boost::filesystem::path& folder, const std::string& appName);

  virtual
  ~ObjectManager();

  /**
   * @brief Creates and saves local file in a local database file
   *
   * Format: /<appname>/file/<hash>/<devicename>/<segment>
   */
  std::tuple<ConstBufferPtr /*object-db name*/, size_t /* number of segments*/>
  localFileToObjects(const boost::filesystem::path& file, const Name& deviceName);

  bool
  objectsToLocalFile(/*in*/ const Name& deviceName, /*in*/ const Buffer& hash,
                     /*out*/ const boost::filesystem::path& file);

private:
  Face& m_face;
  KeyChain& m_keyChain;
  boost::filesystem::path m_folder;
  std::string m_appName;
};

typedef shared_ptr<ObjectManager> ObjectManagerPtr;

namespace error {
struct ObjectManager : virtual boost::exception, virtual std::exception
{
};
} // namespace error

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_OBJECT_MANAGER_HPP
