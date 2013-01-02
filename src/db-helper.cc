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

// Other options: VP_md2, EVP_md5, EVP_sha, EVP_sha1, EVP_sha256, EVP_dss, EVP_dss1, EVP_mdc2, EVP_ripemd160
#define HASH_FUNCTION EVP_sha256

#include <boost/throw_exception.hpp>
typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str; 
// typedef boost::error_info<struct tag_errmsg, int> errmsg_info_int; 

using namespace boost;

const std::string INIT_DATABASE = "\
PRAGMA foreign_keys = ON;                                       \
                                                                \
CREATE TABLE                                                    \
    SyncNodes(                                                  \
        device_id       INTEGER PRIMARY KEY AUTOINCREMENT,      \
        device_name     TEXT NOT NULL,                          \
        description     TEXT,                                   \
        seq_no          INTEGER NOT NULL,                       \
        last_known_tdi  TEXT,                                   \
        last_update     TIMESTAMP                               \
    );                                                          \
                                                                \
CREATE TRIGGER SyncNodesUpdater_trigger                                \
    BEFORE INSERT ON SyncNodes                                         \
    FOR EACH ROW                                                       \
    WHEN (SELECT device_id                                             \
             FROM SyncNodes                                            \
             WHERE device_name=NEW.device_name)                        \
         IS NOT NULL                                                   \
    BEGIN                                                              \
        UPDATE SyncNodes                                               \
            SET seq_no=max(seq_no,NEW.seq_no)                          \
            WHERE device_name=NEW.device_name;                         \
        SELECT RAISE(IGNORE);                                          \
    END;                                                               \
                                                                       \
CREATE INDEX SyncNodes_device_name ON SyncNodes (device_name);         \
                                                                       \
CREATE TABLE SyncLog(                                                  \
        state_id    INTEGER PRIMARY KEY AUTOINCREMENT,                 \
        state_hash  BLOB NOT NULL UNIQUE,                              \
        last_update TIMESTAMP NOT NULL                                 \
    );                                                                 \
                                                                       \
CREATE TABLE                                                            \
    SyncStateNodes(                                                     \
        id          INTEGER PRIMARY KEY AUTOINCREMENT,                  \
        state_id    INTEGER NOT NULL                                    \
            REFERENCES SyncLog (state_id) ON UPDATE CASCADE ON DELETE CASCADE, \
        device_id   INTEGER NOT NULL                                    \
            REFERENCES SyncNodes (device_id) ON UPDATE CASCADE ON DELETE CASCADE, \
        seq_no      INTEGER NOT NULL                                    \
    );                                                                  \
                                                                        \
CREATE INDEX SyncStateNodes_device_id ON SyncStateNodes (device_id);    \
CREATE INDEX SyncStateNodes_state_id  ON SyncStateNodes (state_id);  \
CREATE INDEX SyncStateNodes_seq_no    ON SyncStateNodes (seq_no);          \
                                                                        \
CREATE TRIGGER SyncLogGuard_trigger                                     \
    BEFORE INSERT ON SyncLog                                            \
    FOR EACH ROW                                                        \
    WHEN (SELECT state_hash                                             \
            FROM SyncLog                                                \
            WHERE state_hash=NEW.state_hash)                            \
        IS NOT NULL                                                     \
    BEGIN                                                               \
        DELETE FROM SyncLog WHERE state_hash=NEW.state_hash;            \
    END;                                                                \
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

  res = sqlite3_create_function (m_db, "hash2str", 2, SQLITE_ANY, 0,
                                 DbHelper::hash2str_Func,
                                 0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``hash''"));
    }

  res = sqlite3_create_function (m_db, "str2hash", 2, SQLITE_ANY, 0,
                                 DbHelper::str2hash_Func,
                                 0, 0);
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

HashPtr
DbHelper::RememberStateInStateLog ()
{
  int res = sqlite3_exec (m_db, "BEGIN TRANSACTION;", 0,0,0);

  res += sqlite3_exec (m_db, "\
INSERT INTO SyncLog                                     \
    (state_hash, last_update)                           \
    SELECT                                              \
       hash(device_name, seq_no), datetime('now')       \
    FROM SyncNodes                                      \
    ORDER BY device_name;                               \
", 0,0,0);

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("1"));
    }
  
  sqlite3_int64 rowId = sqlite3_last_insert_rowid (m_db);
  
  sqlite3_stmt *insertStmt;
  res += sqlite3_prepare (m_db, "\
INSERT INTO SyncStateNodes                              \
      (state_id, device_id, seq_no)                     \
      SELECT ?, device_id, seq_no                       \
            FROM SyncNodes;                             \
", -1, &insertStmt, 0);

  res += sqlite3_bind_int64 (insertStmt, 1, rowId);
  sqlite3_step (insertStmt);

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("4"));
    }
  sqlite3_finalize (insertStmt);
  
  sqlite3_stmt *getHashStmt;
  res += sqlite3_prepare (m_db, "\
SELECT state_hash FROM SyncLog WHERE state_id = ?\
", -1, &getHashStmt, 0);
  res += sqlite3_bind_int64 (getHashStmt, 1, rowId);

  HashPtr retval;
  int stepRes = sqlite3_step (getHashStmt);
  if (stepRes == SQLITE_ROW)
    {
      retval = make_shared<Hash> (sqlite3_column_blob (getHashStmt, 0),
                                  sqlite3_column_bytes (getHashStmt, 0));
    }
  sqlite3_finalize (getHashStmt);
  res += sqlite3_exec (m_db, "COMMIT;", 0,0,0);

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with rememberStateInStateLog"));
    }

  return retval;
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

void
DbHelper::hash2str_Func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 1 || sqlite3_value_type (argv[0]) != SQLITE_BLOB)
    {
      sqlite3_result_error (context, "Wrong arguments are supplied for ``hash2str'' function", -1);
      return;
    }

  int hashBytes = sqlite3_value_bytes (argv[0]);
  const void *hash = sqlite3_value_blob (argv[0]);

  std::ostringstream os;
  Hash tmpHash (hash, hashBytes);
  os << tmpHash;
  sqlite3_result_text (context, os.str ().c_str (), -1, SQLITE_TRANSIENT);
}

void
DbHelper::str2hash_Func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 1 || sqlite3_value_type (argv[0]) != SQLITE_TEXT)
    {
      sqlite3_result_error (context, "Wrong arguments are supplied for ``str2hash'' function", -1);
      return;
    }

  size_t hashTextBytes = sqlite3_value_bytes (argv[0]);
  const unsigned char *hashText = sqlite3_value_text (argv[0]);

  Hash hash (std::string (reinterpret_cast<const char*> (hashText), hashTextBytes));
  sqlite3_result_blob (context, hash.GetHash (), hash.GetHashBytes (), SQLITE_TRANSIENT);
}


sqlite3_int64
DbHelper::LookupSyncLog (const std::string &stateHash)
{
  Hash tmpHash (stateHash);
  return LookupSyncLog (stateHash);
}

sqlite3_int64
DbHelper::LookupSyncLog (const Hash &stateHash)
{
  sqlite3_stmt *stmt;
  int res = sqlite3_prepare (m_db, "SELECT state_id FROM SyncLog WHERE state_hash = ?",
                             -1, &stmt, 0);
  
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot prepare statement"));
    }

  res = sqlite3_bind_blob (stmt, 1, stateHash.GetHash (), stateHash.GetHashBytes (), SQLITE_STATIC);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot bind"));
    }

  sqlite3_int64 row = 0; // something bad

  if (sqlite3_step (stmt) == SQLITE_ROW)
    {
      row = sqlite3_column_int64 (stmt, 0);
    }

  sqlite3_finalize (stmt);

  return row;
}

void
DbHelper::UpdateDeviceSeqno (const std::string &name, uint64_t seqNo)
{
  sqlite3_stmt *stmt;
  // update is performed using trigger
  int res = sqlite3_prepare (m_db, "INSERT INTO SyncNodes (device_name, seq_no) VALUES (?,?);", 
                             -1, &stmt, 0);

  res += sqlite3_bind_text  (stmt, 1, name.c_str (), name.size (), SQLITE_STATIC);
  res += sqlite3_bind_int64 (stmt, 2, seqNo);
  sqlite3_step (stmt);
  
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with UpdateDeviceSeqno"));
    }
  sqlite3_finalize (stmt);
}

void
DbHelper::FindStateDifferences (const std::string &oldHash, const std::string &newHash)
{
  Hash tmpOldHash (oldHash);
  Hash tmpNewHash (newHash);

  FindStateDifferences (tmpOldHash, tmpNewHash);
}

void
DbHelper::FindStateDifferences (const Hash &oldHash, const Hash &newHash)
{
  sqlite3_stmt *stmt;

  int res = sqlite3_prepare_v2 (m_db, "\
SELECT sn.device_name, s_old.seq_no, s_new.seq_no                       \
    FROM (SELECT *                                                      \
            FROM SyncStateNodes                                         \
            WHERE state_id=(SELECT state_id                             \
                                FROM SyncLog                            \
                                WHERE state_hash=:old_hash)) s_old      \
    LEFT JOIN (SELECT *                                                 \
                FROM SyncStateNodes                                     \
                WHERE state_id=(SELECT state_id                         \
                                    FROM SyncLog                        \
                                    WHERE state_hash=:new_hash)) s_new  \
                                                                        \
        ON s_old.device_id = s_new.device_id                            \
    JOIN SyncNodes sn ON sn.device_id = s_old.device_id                 \
                                                                        \
    WHERE s_new.seq_no IS NULL OR                                       \
          s_old.seq_no != s_new.seq_no                                  \
                                                                        \
UNION ALL                                                               \
                                                                        \
SELECT sn.device_name, s_old.seq_no, s_new.seq_no                       \
    FROM (SELECT *                                                      \
            FROM SyncStateNodes                                         \
            WHERE state_id=(SELECT state_id                             \
                                FROM SyncLog                            \
                                WHERE state_hash=:new_hash )) s_new     \
    LEFT JOIN (SELECT *                                                 \
                FROM SyncStateNodes                                     \
                WHERE state_id=(SELECT state_id                         \
                                    FROM SyncLog                        \
                                    WHERE state_hash=:old_hash)) s_old  \
                                                                        \
        ON s_old.device_id = s_new.device_id                            \
    JOIN SyncNodes sn ON sn.device_id = s_new.device_id                 \
                                                                        \
    WHERE s_old.seq_no IS NULL                                          \
", -1, &stmt, 0);

  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with FindStateDifferences"));
    }

  res += sqlite3_bind_blob  (stmt, 1, oldHash.GetHash (), oldHash.GetHashBytes (), SQLITE_STATIC);
  res += sqlite3_bind_blob  (stmt, 2, newHash.GetHash (), newHash.GetHashBytes (), SQLITE_STATIC);

  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      std::cout << sqlite3_column_text (stmt, 0) <<
        ": from "  << sqlite3_column_int64 (stmt, 1) <<
        " to "     << sqlite3_column_int64 (stmt, 2) <<
        std::endl;
    }
  sqlite3_finalize (stmt);
  
}
