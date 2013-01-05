/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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

#include "sync-log.h"

#include <boost/make_shared.hpp>

using namespace boost;
using namespace std;

SyncLog::SyncLog (const std::string &path, const std::string &localName)
  : DbHelper (path)
  , m_localName (localName)
{
  SyncLog::RememberStateInStateLog ();

  UpdateDeviceSeqno (localName, 0);
  
  sqlite3_stmt *stmt;
  int res = sqlite3_prepare_v2 (m_db, "SELECT device_id, seq_no FROM SyncNodes WHERE device_name=?", -1, &stmt, 0);

  Ccnx::CcnxCharbufPtr name = m_localName;
  sqlite3_bind_blob (stmt, 1, name->buf (), name->length (), SQLITE_STATIC);

  if (sqlite3_step (stmt) == SQLITE_ROW)
    {
      m_localDeviceId = sqlite3_column_int64 (stmt, 0);
    }
  else
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Impossible thing in SyncLog::SyncLog"));
    }
  sqlite3_finalize (stmt);
}


sqlite3_int64
SyncLog::GetNextLocalSeqNo ()
{
  sqlite3_stmt *stmt_seq;
  sqlite3_prepare_v2 (m_db, "SELECT seq_no FROM SyncNodes WHERE device_id = ?", -1, &stmt_seq, 0);
  sqlite3_bind_int64 (stmt_seq, 1, m_localDeviceId);

  if (sqlite3_step (stmt_seq) != SQLITE_ROW)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Impossible thing in ActionLog::AddActionUpdate"));
    }
  
  sqlite3_int64 seq_no = sqlite3_column_int64 (stmt_seq, 0) + 1; 
  sqlite3_finalize (stmt_seq);

  UpdateDeviceSeqNo (m_localDeviceId, seq_no);
  
  return seq_no;
}


HashPtr
SyncLog::RememberStateInStateLog ()
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
                             << errmsg_info_str (sqlite3_errmsg(m_db)));
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
                             << errmsg_info_str (sqlite3_errmsg(m_db)));
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

sqlite3_int64
SyncLog::LookupSyncLog (const std::string &stateHash)
{
  return LookupSyncLog (*Hash::FromString (stateHash));
}

sqlite3_int64
SyncLog::LookupSyncLog (const Hash &stateHash)
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
SyncLog::UpdateDeviceSeqno (const Ccnx::Name &name, sqlite3_int64 seqNo)
{
  sqlite3_stmt *stmt;
  // update is performed using trigger
  int res = sqlite3_prepare (m_db, "INSERT INTO SyncNodes (device_name, seq_no) VALUES (?,?);", 
                             -1, &stmt, 0);

  Ccnx::CcnxCharbufPtr nameBuf = name;
  res += sqlite3_bind_blob  (stmt, 1, nameBuf->buf (), nameBuf->length (), SQLITE_STATIC);
  res += sqlite3_bind_int64 (stmt, 2, seqNo);
  sqlite3_step (stmt);
  
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with UpdateDeviceSeqno (name)"));
    }
  sqlite3_finalize (stmt);
}

void
SyncLog::UpdateDeviceSeqNo (sqlite3_int64 deviceId, sqlite3_int64 seqNo)
{
  sqlite3_stmt *stmt;
  // update is performed using trigger
  int res = sqlite3_prepare (m_db, "UPDATE SyncNodes SET seq_no=MAX(seq_no,?) WHERE device_id=?;", 
                             -1, &stmt, 0);

  res += sqlite3_bind_int64 (stmt, 1, seqNo);
  res += sqlite3_bind_int64 (stmt, 2, deviceId);
  sqlite3_step (stmt);
  
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Some error with UpdateDeviceSeqNo (id)"));
    }
  sqlite3_finalize (stmt);
}


SyncStateMsgPtr
SyncLog::FindStateDifferences (const std::string &oldHash, const std::string &newHash)
{
  return FindStateDifferences (*Hash::FromString (oldHash), *Hash::FromString (newHash));
}

SyncStateMsgPtr
SyncLog::FindStateDifferences (const Hash &oldHash, const Hash &newHash)
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

  SyncStateMsgPtr msg = make_shared<SyncStateMsg> ();
  
  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      SyncState *state = msg->add_state ();

      state->set_name (reinterpret_cast<const char*> (sqlite3_column_blob (stmt, 0), sqlite3_column_bytes (stmt, 0)));

      sqlite3_int64 newSeqNo = sqlite3_column_int64 (stmt, 2);
      if (newSeqNo > 0)
        {
          state->set_type (SyncState::UPDATE);
          state->set_seq (newSeqNo);
        }
      else
        state->set_type (SyncState::DELETE);

      // std::cout << sqlite3_column_text (stmt, 0) <<
      //   ": from "  << sqlite3_column_int64 (stmt, 1) <<
      //   " to "     << sqlite3_column_int64 (stmt, 2) <<
      //   std::endl;
    }
  sqlite3_finalize (stmt);

  return msg;
}
