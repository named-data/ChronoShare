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

#include "db-helper.h"
// #include "sync-log.h"
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

using namespace boost;

const std::string INIT_DATABASE = "\
PRAGMA foreign_keys = ON;                                       \n\
                                                                \n\
CREATE TABLE                                                    \n\
    SyncNodes(                                                  \n\
        device_id       INTEGER PRIMARY KEY AUTOINCREMENT,      \n\
        device_name     TEXT NOT NULL,                          \n\
        description     TEXT,                                   \n\
        seq_no          INTEGER NOT NULL,                       \n\
        last_known_tdi  TEXT,                                   \n\
        last_update     TIMESTAMP                               \n\
    );                                                          \n\
                                                                \n\
CREATE TRIGGER SyncNodesUpdater_trigger                                \n\
    BEFORE INSERT ON SyncNodes                                         \n\
    FOR EACH ROW                                                       \n\
    WHEN (SELECT device_id                                             \n\
             FROM SyncNodes                                            \n\
             WHERE device_name=NEW.device_name)                        \n\
         IS NOT NULL                                                   \n\
    BEGIN                                                              \n\
        UPDATE SyncNodes                                               \n\
            SET seq_no=max(seq_no,NEW.seq_no)                          \n\
            WHERE device_name=NEW.device_name;                         \n\
        SELECT RAISE(IGNORE);                                          \n\
    END;                                                               \n\
                                                                       \n\
CREATE INDEX SyncNodes_device_name ON SyncNodes (device_name);         \n\
                                                                       \n\
CREATE TABLE SyncLog(                                                  \n\
        state_id    INTEGER PRIMARY KEY AUTOINCREMENT,                 \n\
        state_hash  BLOB NOT NULL UNIQUE,                              \n\
        last_update TIMESTAMP NOT NULL                                 \n\
    );                                                                 \n\
                                                                       \n\
CREATE TABLE                                                            \n\
    SyncStateNodes(                                                     \n\
        id          INTEGER PRIMARY KEY AUTOINCREMENT,                  \n\
        state_id    INTEGER NOT NULL                                    \n\
            REFERENCES SyncLog (state_id) ON UPDATE CASCADE ON DELETE CASCADE, \n\
        device_id   INTEGER NOT NULL                                    \n\
            REFERENCES SyncNodes (device_id) ON UPDATE CASCADE ON DELETE CASCADE, \n\
        seq_no      INTEGER NOT NULL                                    \n\
    );                                                                  \n\
                                                                        \n\
CREATE INDEX SyncStateNodes_device_id ON SyncStateNodes (device_id);    \n\
CREATE INDEX SyncStateNodes_state_id  ON SyncStateNodes (state_id);     \n\
CREATE INDEX SyncStateNodes_seq_no    ON SyncStateNodes (seq_no);       \n\
                                                                        \n\
CREATE TRIGGER SyncLogGuard_trigger                                     \n\
    BEFORE INSERT ON SyncLog                                            \n\
    FOR EACH ROW                                                        \n\
    WHEN (SELECT state_hash                                             \n\
            FROM SyncLog                                                \n\
            WHERE state_hash=NEW.state_hash)                            \n\
        IS NOT NULL                                                     \n\
    BEGIN                                                               \n\
        DELETE FROM SyncLog WHERE state_hash=NEW.state_hash;            \n\
    END;                                                                \n\
                                                                        \n\
CREATE TABLE ActionLog (                                                \n\
    device_id   INTEGER NOT NULL,                                       \n\
    seq_no      INTEGER NOT NULL,                                       \n\
                                                                        \n\
    action      CHAR(1) NOT NULL, /* 0 for \"update\", 1 for \"delete\". */ \n\
    filename    TEXT NOT NULL,                                          \n\
                                                                        \n\
    version     INTEGER NOT NULL,                                       \n\
    action_timestamp TIMESTAMP NOT NULL,                                \n\
                                                                        \n\
    file_hash   BLOB, /* NULL if action is \"delete\" */                \n\
    file_atime  TIMESTAMP,                                              \n\
    file_mtime  TIMESTAMP,                                              \n\
    file_ctime  TIMESTAMP,                                              \n\
    file_chmod  INTEGER,                                                \n\
                                                                        \n\
    parent_device_id INTEGER,                                           \n\
    parent_seq_no    INTEGER,                                           \n\
                                                                        \n\
    action_name	     TEXT,                                              \n\
    action_content_object BLOB,                                         \n\
                                                                        \n\
    PRIMARY KEY (device_id, seq_no),                                    \n\
                                                                        \n\
    FOREIGN KEY (parent_device_id, parent_seq_no)                       \n\
	REFERENCES ActionLog (device_id, seq_no)                        \n\
	ON UPDATE RESTRICT                                              \n\
	ON DELETE SET NULL                                              \n\
);                                                                      \n\
                                                                        \n\
CREATE INDEX ActionLog_filename_version ON ActionLog (filename,version);        \n\
CREATE INDEX ActionLog_parent ON ActionLog (parent_device_id, parent_seq_no);   \n\
CREATE INDEX ActionLog_action_name ON ActionLog (action_name);          \n\
";

DbHelper::DbHelper (const std::string &path)
{
  int res = sqlite3_open((path+"chronoshare.db").c_str (), &m_db);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot open/create dabatabase: [" + path + "chronoshare.db" + "]"));
    }
  
  res = sqlite3_create_function (m_db, "hash", 2, SQLITE_ANY, 0, 0,
                                 DbHelper::hash_xStep, DbHelper::hash_xFinal);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``hash''"));
    }

  // Alex: determine if tables initialized. if not, initialize... not sure what is the best way to go...
  // for now, just attempt to create everything

  char *errmsg = 0;
  res = sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, &errmsg);
  if (res != SQLITE_OK && errmsg != 0)
    {
      std::cerr << "DEBUG: " << errmsg << std::endl;
      sqlite3_free (errmsg);
    }
}

DbHelper::~DbHelper ()
{
  int res = sqlite3_close (m_db);
  if (res != SQLITE_OK)
    {
      // complain
    }
}

void
DbHelper::hash_xStep (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 2)
    {
      // _LOG_ERROR ("Wrong arguments are supplied for ``hash'' function");
      sqlite3_result_error (context, "Wrong arguments are supplied for ``hash'' function", -1);
      return;
    }
  if (sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
      sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
    {
      // _LOG_ERROR ("Hash expects (text,integer) parameters");
      sqlite3_result_error (context, "Hash expects (text,integer) parameters", -1);
      return;
    }
  
  EVP_MD_CTX **hash_context = reinterpret_cast<EVP_MD_CTX **> (sqlite3_aggregate_context (context, sizeof (EVP_MD_CTX *)));

  if (hash_context == 0)
    {
      sqlite3_result_error_nomem (context);
      return;
    }

  if (*hash_context == 0)
    {
      *hash_context = EVP_MD_CTX_create ();
      EVP_DigestInit_ex (*hash_context, HASH_FUNCTION (), 0);
    }
  
  int nameBytes = sqlite3_value_bytes (argv[0]);
  const unsigned char *name = sqlite3_value_text (argv[0]);
  sqlite3_int64 seqno = sqlite3_value_int64 (argv[1]);

  EVP_DigestUpdate (*hash_context, name, nameBytes);
  EVP_DigestUpdate (*hash_context, &seqno, sizeof(sqlite3_int64));
}

void
DbHelper::hash_xFinal (sqlite3_context *context)
{
  EVP_MD_CTX **hash_context = reinterpret_cast<EVP_MD_CTX **> (sqlite3_aggregate_context (context, sizeof (EVP_MD_CTX *)));

  if (hash_context == 0)
    {
      sqlite3_result_error_nomem (context);
      return;
    }

  if (*hash_context == 0) // no rows
    {
      char charNullResult = 0;
      sqlite3_result_blob (context, &charNullResult, 1, SQLITE_TRANSIENT); //SQLITE_TRANSIENT forces to make a copy
      return;
    }
  
  unsigned char *hash = new unsigned char [EVP_MAX_MD_SIZE];
  unsigned int hashLength = 0;

  int ok = EVP_DigestFinal_ex (*hash_context,
			       hash, &hashLength);

  sqlite3_result_blob (context, hash, hashLength, SQLITE_TRANSIENT); //SQLITE_TRANSIENT forces to make a copy
  delete [] hash;
  
  EVP_MD_CTX_destroy (*hash_context);
}



