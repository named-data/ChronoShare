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

#include "action-log.h"

using namespace boost;
using namespace std;

ActionLog::ActionLog (const std::string &path, const std::string &localName)
  : SyncLog (path)
  , m_localName (localName)
{
}

// local add action. remote action is extracted from content object
void
ActionLog::AddActionUpdate (const std::string &filename,
                            const Hash &hash,
                            const std::string &atime, const std::string &mtime, const std::string &ctime,
                            int mode)
{
  int res = sqlite3_exec (m_db, "BEGIN TRANSACTION;", 0,0,0);

  // sqlite3_stmt *stmt;
  // res += sqlite3_prepare_v2 (m_db, "SELECT version,device_id,seq_no FROM ActionLog WHERE filename=? ORDER BY version DESC,device_id DESC LIMIT 1", -1, &stmt, 0);

  // if (res != SQLITE_OK)
  //   {
  //     BOOST_THROW_EXCEPTION (Error::Db ()
  //                            << errmsg_info_str ("Some error with AddActionUpdate"));
  //   }

  // sqlite3_bind_text (stmt, 1, filename.c_str (), -1);
  // if (sqlite3_step (stmt) == SQLITE_ROW)
  //   {
  //     // with parent
  //     sqlite3_int64 version = sqlite3_value_int64 (stmt, 0) + 1;
  //     sqlite3_int64 device_id = sqlite3_value_int64 (stmt, 1);
  //     sqlite3_int64 seq_no = sqlite3_value_int64 (stmt, 0);

      
  //   }
  // else
  //   {
  //     // without parent
      
  //   }

  res += sqlite3_exec (m_db, "END TRANSACTION;", 0,0,0);
}

void
ActionLog::AddActionMove (const std::string &oldFile, const std::string &newFile)
{
}

void
ActionLog::AddActionDelete (const std::string &filename)
{
}
