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

#include "file-state.h"
#include "logging.h"

INIT_LOGGER ("FileState");

using namespace boost;
using namespace std;

const std::string INIT_DATABASE = "\
                                                                        \n\
CREATE TABLE FileState (                                                \n\
    type        INTEGER NOT NULL, /* 0 - newest, 1 - oldest */          \n\
    filename    TEXT NOT NULL,                                          \n\
    directory   TEXT,                                                   \n\
    device_name BLOB NOT NULL,                                          \n\
    seq_no      INTEGER NOT NULL,                                       \n\
    file_hash   BLOB NOT NULL,                                          \n\
    file_atime  TIMESTAMP,                                              \n\
    file_mtime  TIMESTAMP,                                              \n\
    file_ctime  TIMESTAMP,                                              \n\
    file_chmod  INTEGER,                                                \n\
    file_seg_num INTEGER,                                               \n\
    is_complete INTEGER,                                               \n\
                                                                        \n\
    PRIMARY KEY (type, filename)                                        \n\
);                                                                      \n\
                                                                        \n\
CREATE INDEX FileState_device_name_seq_no ON FileState (device_name, seq_no); \n\
CREATE INDEX FileState_type_file_hash ON FileState (type, file_hash);   \n\
";

FileState::FileState (const boost::filesystem::path &path)
  : DbHelper (path / ".chronoshare", "file-state.db")
{
  sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, NULL);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

  int res = sqlite3_create_function (m_db, "directory_name", -1, SQLITE_ANY, 0,
                                     FileState::directory_name_xFun,
                                     0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``directory_name''"));
    }
}

FileState::~FileState ()
{
}

void
FileState::UpdateFile (const std::string &filename, const Hash &hash, const Ccnx::CcnxCharbuf &device_name, sqlite3_int64 seq_no,
                       time_t atime, time_t mtime, time_t ctime, int mode, int seg_num)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db, "UPDATE FileState "
                      "SET "
                      "device_name=?, seq_no=?, "
                      "file_hash=?,"
                      "file_atime=datetime(?, 'unixepoch'),"
                      "file_mtime=datetime(?, 'unixepoch'),"
                      "file_ctime=datetime(?, 'unixepoch'),"
                      "file_chmod=?, "
                      "file_seg_num=? "
                      "WHERE type=0 AND filename=?", -1, &stmt, 0);

  sqlite3_bind_blob  (stmt, 1, device_name.buf (), device_name.length (), SQLITE_TRANSIENT);
  sqlite3_bind_int64 (stmt, 2, seq_no);
  sqlite3_bind_blob  (stmt, 3, hash.GetHash (), hash.GetHashBytes (), SQLITE_TRANSIENT);
  sqlite3_bind_int64 (stmt, 4, atime);
  sqlite3_bind_int64 (stmt, 5, mtime);
  sqlite3_bind_int64 (stmt, 6, ctime);
  sqlite3_bind_int   (stmt, 7, mode);
  sqlite3_bind_int   (stmt, 8, seg_num);
  sqlite3_bind_text  (stmt, 9, filename.c_str (), -1, SQLITE_TRANSIENT);

  sqlite3_step (stmt);

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_ROW && sqlite3_errcode (m_db) != SQLITE_DONE,
                   sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  int affected_rows = sqlite3_changes (m_db);
  if (affected_rows == 0) // file didn't exist
    {
      sqlite3_stmt *stmt;
      sqlite3_prepare_v2 (m_db, "INSERT INTO FileState "
                          "(type,filename,device_name,seq_no,file_hash,file_atime,file_mtime,file_ctime,file_chmod,file_seg_num) "
                          "VALUES (0, ?, ?, ?, ?, "
                          "datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?, ?)", -1, &stmt, 0);

      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_text  (stmt, 1, filename.c_str (), -1, SQLITE_STATIC);
      sqlite3_bind_blob  (stmt, 2, device_name.buf (), device_name.length (), SQLITE_STATIC);
      sqlite3_bind_int64 (stmt, 3, seq_no);
      sqlite3_bind_blob  (stmt, 4, hash.GetHash (), hash.GetHashBytes (), SQLITE_STATIC);
      sqlite3_bind_int64 (stmt, 5, atime);
      sqlite3_bind_int64 (stmt, 6, mtime);
      sqlite3_bind_int64 (stmt, 7, ctime);
      sqlite3_bind_int   (stmt, 8, mode);
      sqlite3_bind_int   (stmt, 9, seg_num);
      // sqlite3_bind_text  (stmt, 10, filename.c_str (), -1, SQLITE_STATIC);

      sqlite3_step (stmt);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE,
                       sqlite3_errmsg (m_db));
      sqlite3_finalize (stmt);

      sqlite3_prepare_v2 (m_db, "UPDATE FileState SET directory=directory_name(filename) WHERE filename=?", -1, &stmt, 0);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_text  (stmt, 1, filename.c_str (), -1, SQLITE_STATIC);
      sqlite3_step (stmt);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE,
                       sqlite3_errmsg (m_db));
      sqlite3_finalize (stmt);
    }
}


void
FileState::DeleteFile (const std::string &filename)
{

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db, "DELETE FROM FileState WHERE type=0 AND filename=?", -1, &stmt, 0);
  sqlite3_bind_text (stmt, 1, filename.c_str (), -1, SQLITE_STATIC);

  _LOG_DEBUG ("Delete " << filename);

  sqlite3_step (stmt);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE,
                   sqlite3_errmsg (m_db));
  sqlite3_finalize (stmt);
}


void
FileState::SetFileComplete (const std::string &filename)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db,
                      "UPDATE FileState SET is_complete=1 WHERE type = 0 AND filename = ?", -1, &stmt, 0);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);

  sqlite3_step (stmt);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);
}


/**
 * @todo Implement checking modification time and permissions
 */
FileItemPtr
FileState::LookupFile (const std::string &filename)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db,
                      "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                      "       FROM FileState "
                      "       WHERE type = 0 AND filename = ?", -1, &stmt, 0);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);

  FileItemPtr retval;
  if (sqlite3_step (stmt) == SQLITE_ROW)
  {
    retval = make_shared<FileItem> ();
    retval->set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
    retval->set_device_name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1));
    retval->set_seq_no      (sqlite3_column_int64 (stmt, 2));
    retval->set_file_hash   (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3));
    retval->set_mtime       (sqlite3_column_int   (stmt, 4));
    retval->set_mode        (sqlite3_column_int   (stmt, 5));
    retval->set_seg_num     (sqlite3_column_int64 (stmt, 6));
    retval->set_is_complete (sqlite3_column_int   (stmt, 7));
  }
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));
  sqlite3_finalize (stmt);

  return retval;
}

FileItemsPtr
FileState::LookupFilesForHash (const Hash &hash)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db,
                      "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                      "   FROM FileState "
                      "   WHERE type = 0 AND file_hash = ?", -1, &stmt, 0);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
  sqlite3_bind_blob(stmt, 1, hash.GetHash (), hash.GetHashBytes (), SQLITE_STATIC);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

  FileItemsPtr retval = make_shared<FileItems> ();
  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      FileItem file;
      file.set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      file.set_device_name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 2));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3));
      file.set_mtime       (sqlite3_column_int   (stmt, 4));
      file.set_mode        (sqlite3_column_int   (stmt, 5));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 6));
      file.set_is_complete (sqlite3_column_int   (stmt, 7));

      retval->push_back (file);
    }
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  return retval;
}


FileItemsPtr
FileState::LookupFilesInFolder (const std::string &folder)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db,
                      "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                      "   FROM FileState "
                      "   WHERE type = 0 AND directory = ?", -1, &stmt, 0);
  if (folder.size () == 0)
    sqlite3_bind_null (stmt, 1);
  else
    sqlite3_bind_text (stmt, 1, folder.c_str (), folder.size (), SQLITE_STATIC);

  FileItemsPtr retval = make_shared<FileItems> ();
  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      FileItem file;
      file.set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      file.set_device_name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 2));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3));
      file.set_mtime       (sqlite3_column_int   (stmt, 4));
      file.set_mode        (sqlite3_column_int   (stmt, 5));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 6));
      file.set_is_complete (sqlite3_column_int   (stmt, 7));

      retval->push_back (file);
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  return retval;
}

FileItemsPtr
FileState::LookupFilesInFolderRecursively (const std::string &folder)
{
  _LOG_DEBUG ("LookupFilesInFolderRecursively: [" << folder << "]");

  sqlite3_stmt *stmt;
  if (folder != "")
    {
      sqlite3_prepare_v2 (m_db,
                          "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                          "   FROM FileState "
                          "   WHERE type = 0 AND (directory = ? OR directory LIKE ?)", -1, &stmt, 0);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_text (stmt, 1, folder.c_str (), folder.size (), SQLITE_STATIC);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      ostringstream escapedFolder;
      for (string::const_iterator ch = folder.begin (); ch != folder.end (); ch ++)
        {
          if (*ch == '%')
            escapedFolder << "\\%";
          else
            escapedFolder << *ch;
        }
      escapedFolder << "/" << "%";
      string escapedFolderStr = escapedFolder.str ();

      sqlite3_bind_text (stmt, 2, escapedFolderStr.c_str (), escapedFolderStr.size (), SQLITE_STATIC);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
    }
  else
    {
      sqlite3_prepare_v2 (m_db,
                          "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                          "   FROM FileState "
                          "   WHERE type = 0", -1, &stmt, 0);
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

  FileItemsPtr retval = make_shared<FileItems> ();
  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      FileItem file;
      file.set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      file.set_device_name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 2));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3));
      file.set_mtime       (sqlite3_column_int   (stmt, 4));
      file.set_mode        (sqlite3_column_int   (stmt, 5));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 6));
      file.set_is_complete (sqlite3_column_int   (stmt, 7));

      retval->push_back (file);
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  return retval;
}

void
FileState::directory_name_xFun (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 1)
    {
      sqlite3_result_error (context, "``directory_name'' expects 1 text argument", -1);
      sqlite3_result_null (context);
      return;
    }

  if (sqlite3_value_bytes (argv[0]) == 0)
    {
      sqlite3_result_null (context);
      return;
    }

  filesystem::path filePath (string (reinterpret_cast<const char*> (sqlite3_value_text (argv[0])), sqlite3_value_bytes (argv[0])));
  string dirPath = filePath.parent_path ().generic_string ();
  // _LOG_DEBUG ("directory_name FUN: " << dirPath);
  if (dirPath.size () == 0)
    {
      sqlite3_result_null (context);
    }
  else
    {
      sqlite3_result_text (context, dirPath.c_str (), dirPath.size (), SQLITE_TRANSIENT);
    }
}
