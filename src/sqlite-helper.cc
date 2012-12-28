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

#include "sqlite-helper.h"
// #include "sync-log.h"

// Other options: VP_md2, EVP_md5, EVP_sha, EVP_sha1, EVP_sha256, EVP_dss, EVP_dss1, EVP_mdc2, EVP_ripemd160
#define HASH_FUNCTION EVP_sha256

#include <boost/throw_exception.hpp>
typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str; 
// typedef boost::error_info<struct tag_errmsg, int> errmsg_info_int; 


const std::string INIT_DATABASE = "\
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
CREATE INDEX SyncNodes_device_name ON SyncNodes (device_name);  \
                                                                \
CREATE TABLE SyncLog(                                           \
        state_hash  BLOB NOT NULL PRIMARY KEY,                  \
        last_update TIMESTAMP NOT NULL                          \
    );                                                          \
                                                                \
CREATE TABLE                                                    \
    SyncStateNodes(                                             \
        id          INTEGER PRIMARY KEY AUTOINCREMENT,          \
        state_hash  BLOB NOT NULL                                       \
            REFERENCES SyncLog (state_hash) ON UPDATE CASCADE ON DELETE CASCADE, \
        device_id   INTEGER NOT NULL                                    \
            REFERENCES SyncNodes (device_id) ON UPDATE CASCADE ON DELETE CASCADE, \
        seq_no      INTEGER NOT NULL                                    \
    );                                                                  \
                                                                        \
CREATE INDEX SyncStateNodes_device_id ON SyncStateNodes (device_id);    \
CREATE INDEX SyncStateNodes_state_hash ON SyncStateNodes (state_hash);  \
CREATE INDEX SyncStateNodes_seq_no ON SyncStateNodes (seq_no);          \
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

  // Alex: determine if tables initialized. if not, initialize... not sure what is the best way to go...
  // for now, just attempt to create everything

  char *errmsg = 0;
  res = sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, &errmsg);
  // if (res != SQLITE_OK && errmsg != 0)
  //   {
  //     std::cerr << "DEBUG: " << errmsg << std::endl;
  //     sqlite3_free (errmsg);
  //   }

  res = sqlite3_exec (m_db, "INSERT INTO SyncLog (state_hash, last_update) SELECT hash(device_name, seq_no), datetime('now') FROM SyncNodes ORDER BY device_name",
                          NULL, NULL, &errmsg);
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
      sqlite3_result_null (context);
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
