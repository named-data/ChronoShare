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

#ifndef OBJECT_DB_H
#define OBJECT_DB_H

#include <string>
#include <sqlite3.h>
#include <ccnx-common.h>
#include <ccnx-name.h>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>

class ObjectDb
{
public:
  // database will be create in <folder>/<first-pair-of-hash-bytes>/<rest-of-hash>
  ObjectDb (const boost::filesystem::path &folder, const std::string &hash);
  ~ObjectDb ();

  void
  saveContentObject (const Ccnx::Name &deviceName, sqlite3_int64 segment, const Ccnx::Bytes &data);

  Ccnx::BytesPtr
  fetchSegment (const Ccnx::Name &deviceName, sqlite3_int64 segment);

  // sqlite3_int64
  // getNumberOfSegments (const Ccnx::Name &deviceName);

  static bool
  DoesExist (const boost::filesystem::path &folder, const Ccnx::Name &deviceName, const std::string &hash);
  
private:
  sqlite3 *m_db;
};

typedef boost::shared_ptr<ObjectDb> ObjectDbPtr;

#endif // OBJECT_DB_H
