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
#include <ccnx-wrapper.h>
#include <hash-helper.h>
#include <boost/filesystem.hpp>

// everything related to managing object files

class ObjectManager
{
public:
  ObjectManager (Ccnx::CcnxWrapperPtr ccnx, const Ccnx::Name &localDeviceName, const boost::filesystem::path &folder);
  virtual ~ObjectManager ();

  HashPtr
  storeLocalFile (const boost::filesystem::path &file);
  
private:
  Ccnx::CcnxWrapperPtr m_ccnx;
  Ccnx::Name m_localDeviceName;
  boost::filesystem::path m_folder;
};

typedef boost::shared_ptr<ObjectManager> ObjectManagerPtr;

namespace Error {
struct ObjectManager : virtual boost::exception, virtual std::exception { };
}

#endif // OBJECT_MANAGER_H
