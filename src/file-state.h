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

#ifndef FILE_STATE_H
#define FILE_STATE_H

#include "file-item.pb.h"
#include "hash-helper.h"

#include <boost/tuple/tuple.hpp>
#include <boost/exception/all.hpp>

#include <list>

typedef std::list<FileItem> FileItems;
typedef boost::shared_ptr<FileItem>  FileItemPtr;
typedef boost::shared_ptr<FileItems> FileItemsPtr;


class FileState
{
public:
  virtual FileItemPtr
  LookupFile (const std::string &filename) = 0;

  virtual FileItemsPtr
  LookupFilesForHash (const Hash &hash) = 0;

  virtual FileItemsPtr
  LookupFilesInFolder (const std::string &folder) = 0;

  virtual FileItemsPtr
  LookupFilesInFolderRecursively (const std::string &folder) = 0;
};

namespace Error {
struct FileState : virtual boost::exception, virtual std::exception { };
}


#endif // ACTION_LOG_H
