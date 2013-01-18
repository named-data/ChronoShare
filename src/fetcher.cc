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

#include "fetcher.h"
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

using namespace boost;
using namespace std;
namespace fs = boost::filesystem;

const std::string INIT_DATABASE = "\
PRAGMA foreign_keys = ON;           \
                                    \
CREATE TABLE                                     \
    Queue(                                       \
        name     BLOB NOT NULL PRIMARY KEY,      \
        seq_no_rcvd INTEGER,                     \
        seq_no_available INTEGER,                \
    );                                           \
";

Fetcher::Fetcher (const fs::path &path, const string &name)
{
  fs::path chronoshareDirectory = path / ".chronoshare";
  fs::create_directories (chronoshareDirectory);
  
  int res = sqlite3_open((chronoshareDirectory / (name+".db")).c_str (), &m_db);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Fetcher ()
                             << errmsg_info_str ("Cannot open/create dabatabase: [" + (chronoshareDirectory / (name+".db")).string () + "]"));
    }
  
  char *errmsg = 0;
  res = sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, &errmsg);
  if (res != SQLITE_OK && errmsg != 0)
    {
      // std::cerr << "DEBUG: " << errmsg << std::endl;
      sqlite3_free (errmsg);
    }
}

Fetcher::~Fetcher ()
{
  int res = sqlite3_close (m_db);
  if (res != SQLITE_OK)
    {
      // complain
    }
}
