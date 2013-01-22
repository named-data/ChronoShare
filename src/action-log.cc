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
using namespace Ccnx;

ActionLog::ActionLog (Ccnx::CcnxWrapperPtr ccnx, const boost::filesystem::path &path,
                      const std::string &localName, const std::string &sharedFolder)
  : SyncLog (path, localName)
  , m_ccnx (ccnx)
  , m_sharedFolderName (sharedFolder)
{
  int res = sqlite3_create_function (m_db, "apply_action", -1, SQLITE_ANY, reinterpret_cast<void*> (this),
                                 ActionLog::apply_action_xFun,
                                 0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``apply_action''"));
    }
}

tuple<sqlite3_int64, sqlite3_int64, sqlite3_int64, string>
ActionLog::GetExistingRecord (const std::string &filename)
{
  // check if something already exists
  sqlite3_stmt *stmt;
  int res = sqlite3_prepare_v2 (m_db, "SELECT a.version,a.device_id,a.seq_no,a.action,s.device_name "
                                "FROM ActionLog a JOIN SyncNodes s ON s.device_id = a.device_id "
                                "WHERE filename=? ORDER BY a.version DESC LIMIT 1", -1, &stmt, 0);

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with GetExistingRecord"));
    }

  // with parent with version number + 1
  sqlite3_int64 version = 0;
  sqlite3_int64 parent_device_id = -1;
  sqlite3_int64 parent_seq_no = -1;
  string parent_device_name;

  sqlite3_bind_text (stmt, 1, filename.c_str (), filename.size (), SQLITE_STATIC);
  if (sqlite3_step (stmt) == SQLITE_ROW)
    {
      version = sqlite3_column_int64 (stmt, 0) + 1;

      if (sqlite3_column_int (stmt, 3) == 0) // prevent "linking" if the file was previously deleted
        {
          parent_device_id = sqlite3_column_int64 (stmt, 1);
          parent_seq_no = sqlite3_column_int64 (stmt, 2);
          parent_device_name = string(reinterpret_cast<const char*> (sqlite3_column_blob (stmt, 4)), sqlite3_column_bytes (stmt, 4));
        }
    }

  sqlite3_finalize (stmt);
  return make_tuple (version, parent_device_id, parent_seq_no, parent_device_name);
}

// local add action. remote action is extracted from content object
void
ActionLog::AddActionUpdate (const std::string &filename,
                            const Hash &hash,
                            time_t wtime,
                            int mode,
                            int seg_num)
{
  sqlite3_exec (m_db, "BEGIN TRANSACTION;", 0,0,0);

  sqlite3_int64 seq_no = GetNextLocalSeqNo ();
  sqlite3_int64 version = 0;
  sqlite3_int64 parent_device_id = -1;
  string        parent_device_name;
  sqlite3_int64 parent_seq_no = -1;

  sqlite3_int64 action_time = time (0);

  tie (version, parent_device_id, parent_seq_no, parent_device_name) = GetExistingRecord (filename);

  sqlite3_stmt *stmt;
  int res = sqlite3_prepare_v2 (m_db, "INSERT INTO ActionLog "
                                "(device_id, seq_no, action, filename, version, action_timestamp, "
                                "file_hash, file_atime, file_mtime, file_ctime, file_chmod, file_seg_num, "
                                "parent_device_id, parent_seq_no, "
                                "action_name, action_content_object) "
                                "VALUES (?, ?, ?, ?, ?, datetime(?, 'unixepoch'),"
                                "        ?, datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?,"
                                "        ?, ?, "
                                "        ?, ?);", -1, &stmt, 0);

  // cout << "INSERT INTO ActionLog "
  //                               "(device_id, seq_no, action, filename, version, action_timestamp, "
  //                               "file_hash, file_atime, file_mtime, file_ctime, file_chmod, "
  //                               "parent_device_id, parent_seq_no) "
  //                               "VALUES (?, ?, ?, ?, ?, datetime(?, 'unixepoch'),"
  //                               "        ?, datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?,"
  //   "        ?, ?)" << endl;

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str (sqlite3_errmsg (m_db))
                             );
                             // << errmsg_info_str ("Some error with prepare AddActionUpdate"));
    }


  sqlite3_bind_int64 (stmt, 1, m_localDeviceId);
  sqlite3_bind_int64 (stmt, 2, seq_no);
  sqlite3_bind_int   (stmt, 3, 0);
  sqlite3_bind_text  (stmt, 4, filename.c_str (), filename.size (), SQLITE_TRANSIENT);
  sqlite3_bind_int64 (stmt, 5, version);
  sqlite3_bind_int64 (stmt, 6, action_time);

  sqlite3_bind_blob  (stmt, 7, hash.GetHash (), hash.GetHashBytes (), SQLITE_TRANSIENT);

  // sqlite3_bind_int64 (stmt, 8, atime); // NULL
  sqlite3_bind_int64 (stmt, 9, wtime);
  // sqlite3_bind_int64 (stmt, 10, ctime); // NULL
  sqlite3_bind_int   (stmt, 11, mode);
  sqlite3_bind_int   (stmt, 12, seg_num);

  if (parent_device_id > 0 && parent_seq_no > 0)
    {
      sqlite3_bind_int64 (stmt, 13, parent_device_id);
      sqlite3_bind_int64 (stmt, 14, parent_seq_no);
    }
  else
    {
      sqlite3_bind_null (stmt, 13);
      sqlite3_bind_null (stmt, 14);
    }

  // missing part: creating ContentObject for the action !!!

  ActionItem item;
  item.set_action (ActionItem::UPDATE);
  item.set_filename (filename);
  item.set_version (version);
  item.set_timestamp (action_time);
  item.set_file_hash (hash.GetHash (), hash.GetHashBytes ());
  // item.set_atime (atime);
  item.set_mtime (wtime);
  // item.set_ctime (ctime);
  item.set_mode (mode);
  item.set_seg_num (seg_num);

  if (parent_device_id > 0 && parent_seq_no > 0)
    {
      cout << Name (reinterpret_cast<const unsigned char *> (parent_device_name.c_str ()),
                    parent_device_name.size ()) << endl;

      item.set_parent_device_name (parent_device_name);
      item.set_parent_seq_no (parent_seq_no);
    }

  // assign name to the action, serialize action, and create content object

  string item_msg;
  item.SerializeToString (&item_msg);
  Name actionName = Name (m_localName)("action")(m_sharedFolderName)(seq_no);

  Bytes actionData = m_ccnx->createContentObject (actionName, item_msg.c_str (), item_msg.size ());
  CcnxCharbufPtr namePtr = actionName.toCcnxCharbuf ();

  sqlite3_bind_blob (stmt, 14, namePtr->buf (), namePtr->length (), SQLITE_TRANSIENT);
  sqlite3_bind_blob (stmt, 15, &actionData[0], actionData.size (), SQLITE_TRANSIENT);

  sqlite3_step (stmt);

  sqlite3_finalize (stmt);

  sqlite3_exec (m_db, "END TRANSACTION;", 0,0,0);
}

void
ActionLog::AddActionMove (const std::string &oldFile, const std::string &newFile)
{
  // not supported yet
  BOOST_THROW_EXCEPTION (Error::Db ()
                         << errmsg_info_str ("Move operation is not yet supported"));
}

void
ActionLog::AddActionDelete (const std::string &filename)
{
  sqlite3_exec (m_db, "BEGIN TRANSACTION;", 0,0,0);

  sqlite3_int64 version = 0;
  sqlite3_int64 parent_device_id = -1;
  string        parent_device_name;
  sqlite3_int64 parent_seq_no = -1;

  sqlite3_int64 action_time = time (0);

  tie (version, parent_device_id, parent_seq_no, parent_device_name) = GetExistingRecord (filename);
  if (parent_device_id < 0) // no records exist or file was already deleted
    {
      sqlite3_exec (m_db, "END TRANSACTION;", 0,0,0);
      return;
    }

  sqlite3_int64 seq_no = GetNextLocalSeqNo ();

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db, "INSERT INTO ActionLog "
                      "(device_id, seq_no, action, filename, version, action_timestamp, "
                      "parent_device_id, parent_seq_no, "
                      "action_name, action_content_object) "
                      "VALUES (?, ?, ?, ?, ?, datetime(?, 'unixepoch'),"
                      "        ?, ?,"
                      "        ?, ?)", -1, &stmt, 0);

  sqlite3_bind_int64 (stmt, 1, m_localDeviceId);
  sqlite3_bind_int64 (stmt, 2, seq_no);
  sqlite3_bind_int   (stmt, 3, 1);
  sqlite3_bind_text  (stmt, 4, filename.c_str (), filename.size (), SQLITE_TRANSIENT);
  sqlite3_bind_int64 (stmt, 5, version);
  sqlite3_bind_int64 (stmt, 6, action_time);

  sqlite3_bind_int64 (stmt, 7, parent_device_id);
  sqlite3_bind_int64 (stmt, 8, parent_seq_no);


  ActionItem item;
  item.set_action (ActionItem::UPDATE);
  item.set_filename (filename);
  item.set_version (version);
  item.set_timestamp (action_time);
  item.set_parent_device_name (parent_device_name);
  item.set_parent_seq_no (parent_seq_no);

  string item_msg;
  item.SerializeToString (&item_msg);
  Name actionName = Name (m_localName)("action")(m_sharedFolderName)(seq_no);

  Bytes actionData = m_ccnx->createContentObject (actionName, item_msg.c_str (), item_msg.size ());
  CcnxCharbufPtr namePtr = actionName.toCcnxCharbuf ();

  sqlite3_bind_blob (stmt, 9, namePtr->buf (), namePtr->length (), SQLITE_TRANSIENT);
  sqlite3_bind_blob (stmt, 10, &actionData[0], actionData.size (), SQLITE_TRANSIENT);

  sqlite3_step (stmt);

  // cout << Ccnx::Name (reinterpret_cast<const unsigned char *> (parent_device_name.c_str ()),
  //                     parent_device_name.size ()) << endl;

  // assign name to the action, serialize action, and create content object

  sqlite3_finalize (stmt);

  sqlite3_exec (m_db, "END TRANSACTION;", 0,0,0);
}


void
ActionLog::apply_action_xFun (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  ActionLog *the = reinterpret_cast<ActionLog*> (sqlite3_user_data (context));

  if (argc != 11)
    {
      sqlite3_result_error (context, "``apply_action'' expects 11 arguments", -1);
      return;
    }

  // cout << "apply_function called with " << argc << endl;

  // cout << "device_name: " << sqlite3_value_text (argv[0]) << endl;
  // cout << "action: " << sqlite3_value_int (argv[1]) << endl;
  // cout << "filename: " << sqlite3_value_text (argv[2]) << endl;

  string device_name (reinterpret_cast<const char*> (sqlite3_value_blob (argv[0])), sqlite3_value_bytes (argv[0]));
  sqlite3_int64 device_id = sqlite3_value_int64 (argv[1]);
  sqlite3_int64 seq_no    = sqlite3_value_int64 (argv[2]);
  int action         = sqlite3_value_int  (argv[3]);
  string filename    = reinterpret_cast<const char*> (sqlite3_value_text (argv[4]));

  if (action == 0) // update
    {
      Hash hash (sqlite3_value_blob (argv[5]), sqlite3_value_bytes (argv[5]));
      time_t atime = static_cast<time_t> (sqlite3_value_int64 (argv[6]));
      time_t mtime = static_cast<time_t> (sqlite3_value_int64 (argv[7]));
      time_t ctime = static_cast<time_t> (sqlite3_value_int64 (argv[8]));
      int mode = sqlite3_value_int (argv[9]);
      int seg_num = sqlite3_value_int (argv[10]);

      cout << "Update " << filename << " " << atime << " " << mtime << " " << ctime << " " << hash << endl;

      sqlite3_stmt *stmt;
      sqlite3_prepare_v2 (the->m_db, "UPDATE FileState "
                          "SET "
                          "device_id=?, seq_no=?, "
                          "file_hash=?,"
                          "file_atime=datetime(?, 'unixepoch'),"
                          "file_mtime=datetime(?, 'unixepoch'),"
                          "file_ctime=datetime(?, 'unixepoch'),"
                          "file_chmod=? "
                          "file_seg_num=? "
                          "WHERE type=0 AND filename=?", -1, &stmt, 0);

      sqlite3_bind_int64 (stmt, 1, device_id);
      sqlite3_bind_int64 (stmt, 2, seq_no);
      sqlite3_bind_blob  (stmt, 3, hash.GetHash (), hash.GetHashBytes (), SQLITE_TRANSIENT);
      sqlite3_bind_int64 (stmt, 4, atime);
      sqlite3_bind_int64 (stmt, 5, mtime);
      sqlite3_bind_int64 (stmt, 6, ctime);
      sqlite3_bind_int   (stmt, 7, mode);
      sqlite3_bind_int   (stmt, 8, seg_num);
      sqlite3_bind_text  (stmt, 9, filename.c_str (), -1, SQLITE_TRANSIENT);

      sqlite3_step (stmt);

      // cout << sqlite3_errmsg (the->m_db) << endl;

      sqlite3_finalize (stmt);

      int affected_rows = sqlite3_changes (the->m_db);
      if (affected_rows == 0) // file didn't exist
        {
          sqlite3_stmt *stmt;
          sqlite3_prepare_v2 (the->m_db, "INSERT INTO FileState "
                              "(type,filename,device_id,seq_no,file_hash,file_atime,file_mtime,file_ctime,file_chmod) "
                              "VALUES (0, ?, ?, ?, ?, "
                              "datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?)", -1, &stmt, 0);

          sqlite3_bind_text  (stmt, 1, filename.c_str (), -1, SQLITE_TRANSIENT);
          sqlite3_bind_int64 (stmt, 2, device_id);
          sqlite3_bind_int64 (stmt, 3, seq_no);
          sqlite3_bind_blob  (stmt, 4, hash.GetHash (), hash.GetHashBytes (), SQLITE_TRANSIENT);
          sqlite3_bind_int64 (stmt, 5, atime);
          sqlite3_bind_int64 (stmt, 6, mtime);
          sqlite3_bind_int64 (stmt, 7, ctime);
          sqlite3_bind_int   (stmt, 8, mode);

          sqlite3_step (stmt);
          // cout << sqlite3_errmsg (the->m_db) << endl;
          sqlite3_finalize (stmt);
        }
    }
  else if (action == 1) // delete
    {
      sqlite3_stmt *stmt;
      sqlite3_prepare_v2 (the->m_db, "DELETE FROM FileState WHERE type=0 AND filename=?", -1, &stmt, 0);
      sqlite3_bind_text (stmt, 1, filename.c_str (), -1, SQLITE_STATIC);

      cout << "Delete " << filename << endl;

      sqlite3_step (stmt);
      sqlite3_finalize (stmt);
    }

  sqlite3_result_null (context);
}
