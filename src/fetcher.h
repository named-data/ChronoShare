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

#ifndef FETCHER_H
#define FETCHER_H

#include <stdint.h>
#include <sqlite3.h>
#include <boost/exception/all.hpp>
#include <string>
#include <boost/filesystem.hpp>

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str; 

class Fetcher
{
public:
  Fetcher (const boost::filesystem::path &path, const std::string &name);
  virtual ~Fetcher ();
  
private:

protected:
  sqlite3 *m_db;
};

namespace Error {
struct Fetcher : virtual boost::exception, virtual std::exception { };
}

typedef boost::shared_ptr<Fetcher> FetcherPtr;


#endif // FETCHER_H
