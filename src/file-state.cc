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
#include <boost/bind.hpp>

INIT_LOGGER ("FileState");

using namespace boost;
using namespace std;

const std::string INIT_DATABASE = "\
                                                                        \n\
CREATE TABLE FileState (                                                \n\
    type        INTEGER NOT NULL, /* 0 - newest, 1 - oldest */          \n\
    filename    TEXT NOT NULL,                                          \n\
    version     INTEGER,                                                \n\
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

FileState::FileState (const boost::filesystem::path &path, bool cow)
  : DbHelper (path / ".chronoshare",  (cow ? "file-state-tmp.db" : "file-state.db"))
{
  sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, NULL);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
}

FileState::~FileState ()
{
}

void
FileState::UpdateFile (const std::string &filename, sqlite3_int64 version,
                       const Hash &hash, const Ndnx::NdnxCharbuf &device_name, sqlite3_int64 seq_no,
                       time_t atime, time_t mtime, time_t ctime, int mode, int seg_num)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db, "UPDATE FileState "
                      "SET "
                      "device_name=?, seq_no=?, "
                      "version=?,"
                      "file_hash=?,"
                      "file_atime=datetime(?, 'unixepoch'),"
                      "file_mtime=datetime(?, 'unixepoch'),"
                      "file_ctime=datetime(?, 'unixepoch'),"
                      "file_chmod=?, "
                      "file_seg_num=? "
                      "WHERE type=0 AND filename=?", -1, &stmt, 0);

  sqlite3_bind_blob  (stmt, 1, device_name.buf (), device_name.length (), SQLITE_STATIC);
  sqlite3_bind_int64 (stmt, 2, seq_no);
  sqlite3_bind_int64 (stmt, 3, version);
  sqlite3_bind_blob  (stmt, 4, hash.GetHash (), hash.GetHashBytes (), SQLITE_STATIC);
  sqlite3_bind_int64 (stmt, 5, atime);
  sqlite3_bind_int64 (stmt, 6, mtime);
  sqlite3_bind_int64 (stmt, 7, ctime);
  sqlite3_bind_int   (stmt, 8, mode);
  sqlite3_bind_int   (stmt, 9, seg_num);
  sqlite3_bind_text  (stmt, 10, filename.c_str (), -1, SQLITE_STATIC);

  sqlite3_step (stmt);

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_ROW && sqlite3_errcode (m_db) != SQLITE_DONE,
                   sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  int affected_rows = sqlite3_changes (m_db);
  if (affected_rows == 0) // file didn't exist
    {
      sqlite3_stmt *stmt;
      sqlite3_prepare_v2 (m_db, "INSERT INTO FileState "
                          "(type,filename,version,device_name,seq_no,file_hash,file_atime,file_mtime,file_ctime,file_chmod,file_seg_num) "
                          "VALUES (0, ?, ?, ?, ?, ?, "
                          "datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?, ?)", -1, &stmt, 0);

      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_text  (stmt, 1, filename.c_str (), -1, SQLITE_STATIC);
      sqlite3_bind_int64 (stmt, 2, version);
      sqlite3_bind_blob  (stmt, 3, device_name.buf (), device_name.length (), SQLITE_STATIC);
      sqlite3_bind_int64 (stmt, 4, seq_no);
      sqlite3_bind_blob  (stmt, 5, hash.GetHash (), hash.GetHashBytes (), SQLITE_STATIC);
      sqlite3_bind_int64 (stmt, 6, atime);
      sqlite3_bind_int64 (stmt, 7, mtime);
      sqlite3_bind_int64 (stmt, 8, ctime);
      sqlite3_bind_int   (stmt, 9, mode);
      sqlite3_bind_int   (stmt, 10, seg_num);

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
                      "SELECT filename,version,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                      "       FROM FileState "
                      "       WHERE type = 0 AND filename = ?", -1, &stmt, 0);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);

  FileItemPtr retval;
  if (sqlite3_step (stmt) == SQLITE_ROW)
  {
    retval = make_shared<FileItem> ();
    retval->set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
    retval->set_version     (sqlite3_column_int64 (stmt, 1));
    retval->set_device_name (sqlite3_column_blob  (stmt, 2), sqlite3_column_bytes (stmt, 2));
    retval->set_seq_no      (sqlite3_column_int64 (stmt, 3));
    retval->set_file_hash   (sqlite3_column_blob  (stmt, 4), sqlite3_column_bytes (stmt, 4));
    retval->set_mtime       (sqlite3_column_int   (stmt, 5));
    retval->set_mode        (sqlite3_column_int   (stmt, 6));
    retval->set_seg_num     (sqlite3_column_int64 (stmt, 7));
    retval->set_is_complete (sqlite3_column_int   (stmt, 8));
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
                      "SELECT filename,version,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
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
      file.set_version     (sqlite3_column_int64 (stmt, 1));
      file.set_device_name (sqlite3_column_blob  (stmt, 2), sqlite3_column_bytes (stmt, 2));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 3));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 4), sqlite3_column_bytes (stmt, 4));
      file.set_mtime       (sqlite3_column_int   (stmt, 5));
      file.set_mode        (sqlite3_column_int   (stmt, 6));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 7));
      file.set_is_complete (sqlite3_column_int   (stmt, 8));

      retval->push_back (file);
    }
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  return retval;
}

void
FileState::LookupFilesInFolder (const boost::function<void (const FileItem&)> &visitor, const std::string &folder, int offset/*=0*/, int limit/*=-1*/)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2 (m_db,
                      "SELECT filename,version,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                      "   FROM FileState "
                      "   WHERE type = 0 AND directory = ?"
                      "   LIMIT ? OFFSET ?", -1, &stmt, 0);
  if (folder.size () == 0)
    sqlite3_bind_null (stmt, 1);
  else
    sqlite3_bind_text (stmt, 1, folder.c_str (), folder.size (), SQLITE_STATIC);

  sqlite3_bind_int (stmt, 2, limit);
  sqlite3_bind_int (stmt, 3, offset);

  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      FileItem file;
      file.set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      file.set_version     (sqlite3_column_int64 (stmt, 1));
      file.set_device_name (sqlite3_column_blob  (stmt, 2), sqlite3_column_bytes (stmt, 2));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 3));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 4), sqlite3_column_bytes (stmt, 4));
      file.set_mtime       (sqlite3_column_int   (stmt, 5));
      file.set_mode        (sqlite3_column_int   (stmt, 6));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 7));
      file.set_is_complete (sqlite3_column_int   (stmt, 8));

      visitor (file);
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);
}

FileItemsPtr
FileState::LookupFilesInFolder (const std::string &folder, int offset/*=0*/, int limit/*=-1*/)
{
  FileItemsPtr retval = make_shared<FileItems> ();
  LookupFilesInFolder (boost::bind (&FileItems::push_back, retval.get (), _1), folder, offset, limit);

  return retval;
}

bool
FileState::LookupFilesInFolderRecursively (const boost::function<void (const FileItem&)> &visitor, const std::string &folder, int offset/*=0*/, int limit/*=-1*/)
{
  _LOG_DEBUG ("LookupFilesInFolderRecursively: [" << folder << "]");

  if (limit >= 0)
    limit ++;

  sqlite3_stmt *stmt;
  if (folder != "")
    {
      /// @todo Do something to improve efficiency of this query. Right now it is basically scanning the whole database

      sqlite3_prepare_v2 (m_db,
                          "SELECT filename,version,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                          "   FROM FileState "
                          "   WHERE type = 0 AND is_dir_prefix (?, directory)=1 "
                          "   ORDER BY filename "
                          "   LIMIT ? OFFSET ?", -1, &stmt, 0); // there is a small ambiguity with is_prefix matching, but should be ok for now
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_text (stmt, 1, folder.c_str (), folder.size (), SQLITE_STATIC);
      _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

      sqlite3_bind_int (stmt, 2, limit);
      sqlite3_bind_int (stmt, 3, offset);
    }
  else
    {
      sqlite3_prepare_v2 (m_db,
                          "SELECT filename,version,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,is_complete "
                          "   FROM FileState "
                          "   WHERE type = 0"
                          "   ORDER BY filename "
                          "   LIMIT ? OFFSET ?", -1, &stmt, 0);
      sqlite3_bind_int (stmt, 1, limit);
      sqlite3_bind_int (stmt, 2, offset);
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));

  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      if (limit == 1)
        break;

      FileItem file;
      file.set_filename    (reinterpret_cast<const char *> (sqlite3_column_text  (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      file.set_version     (sqlite3_column_int64 (stmt, 1));
      file.set_device_name (sqlite3_column_blob  (stmt, 2), sqlite3_column_bytes (stmt, 2));
      file.set_seq_no      (sqlite3_column_int64 (stmt, 3));
      file.set_file_hash   (sqlite3_column_blob  (stmt, 4), sqlite3_column_bytes (stmt, 4));
      file.set_mtime       (sqlite3_column_int   (stmt, 5));
      file.set_mode        (sqlite3_column_int   (stmt, 6));
      file.set_seg_num     (sqlite3_column_int64 (stmt, 7));
      file.set_is_complete (sqlite3_column_int   (stmt, 8));

      visitor (file);
      limit --;
    }

  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_DONE, sqlite3_errmsg (m_db));

  sqlite3_finalize (stmt);

  return (limit == 1);
}

FileItemsPtr
FileState::LookupFilesInFolderRecursively (const std::string &folder, int offset/*=0*/, int limit/*=-1*/)
{
  FileItemsPtr retval = make_shared<FileItems> ();
  LookupFilesInFolder (boost::bind (&FileItems::push_back, retval.get (), _1), folder, offset, limit);

  return retval;
}
