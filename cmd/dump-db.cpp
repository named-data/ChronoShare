/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#include "dispatcher.hpp"
#include "fs-watcher.hpp"
#include "logging.hpp"
#include "ccnx-wrapper.hpp"

#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace std;
using namespace Ndnx;

INIT_LOGGER ("DumpDb");

class StateLogDumper : public DbHelper
{
public:
  StateLogDumper (const filesystem::path &path)
  : DbHelper (path / ".chronoshare", "sync-log.db")
  {
  }

  void
  DumpState ()
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db, "SELECT hash(device_name, seq_no) FROM (SELECT * FROM SyncNodes ORDER BY device_name)", -1, &stmt, 0);
    sqlite3_step (stmt);
    Hash hash (sqlite3_column_blob  (stmt, 0), sqlite3_column_bytes (stmt, 0));
    sqlite3_finalize (stmt);

    sqlite3_prepare_v2 (m_db,
                        "SELECT device_name, seq_no, last_known_locator, last_update "
                        "   FROM SyncNodes "
                        "   ORDER BY device_name", -1, &stmt, 0);

    cout.setf(std::ios::left, std::ios::adjustfield);
    cout << ">> SYNC NODES (" << hash.shortHash () << ") <<" << endl;
    cout << "====================================================================================" << endl;
    cout << setw(30) << "device_name" << " | seq_no | " << setw(20) << "locator" << " | last_update " << endl;
    cout << "====================================================================================" << endl;

    while (sqlite3_step (stmt) == SQLITE_ROW)
      {
        cout << setw (30) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 0), sqlite3_column_bytes (stmt, 0))) << " | "; // device_name
        cout << setw (6) << sqlite3_column_int64 (stmt, 1) << " | "; // seq_no
        cout << setw (20) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 2), sqlite3_column_bytes (stmt, 2))) << " | "; // locator
        if (sqlite3_column_bytes (stmt, 3) > 0)
          {
            cout << setw (10) << sqlite3_column_text (stmt, 3) << endl;
          }
        else
          {
            cout << "unknown" << endl;
          }
      }
    sqlite3_finalize (stmt);
  }

  void
  DumpLog ()
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db,
                        "SELECT state_hash, last_update, state_id "
                        "   FROM SyncLog "
                        "   ORDER BY last_update", -1, &stmt, 0);

    cout.setf(std::ios::left, std::ios::adjustfield);
    cout << ">> SYNC LOG <<" << endl;
    cout << "====================================================================================" << endl;
    cout << setw(10) << "state_hash" << " | state details " << endl;
    cout << "====================================================================================" << endl;

    while (sqlite3_step (stmt) == SQLITE_ROW)
      {
        cout << setw (10) << Hash (sqlite3_column_blob  (stmt, 0), sqlite3_column_bytes (stmt, 0)).shortHash () << " | "; // state hash

        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2 (m_db,
                            "SELECT device_name, ss.seq_no "
                            "   FROM SyncStateNodes ss JOIN SyncNodes sn ON ss.device_id = sn.device_id "
                            "   WHERE state_id=? "
                            "   ORDER BY device_name", -1, &stmt2, 0);
        _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
        sqlite3_bind_int64 (stmt2, 1, sqlite3_column_int64 (stmt, 2));

        while (sqlite3_step (stmt2) == SQLITE_ROW)
          {
            cout << lexical_cast<string> (Name (sqlite3_column_blob  (stmt2, 0), sqlite3_column_bytes (stmt2, 0)))
                 << "("
                 << sqlite3_column_int64 (stmt2, 1)
                 << "); ";
          }

        sqlite3_finalize (stmt2);

        cout << endl;
      }
    sqlite3_finalize (stmt);
  }
};

class ActionLogDumper : public DbHelper
{
public:
  ActionLogDumper (const filesystem::path &path)
  : DbHelper (path / ".chronoshare", "action-log.db")
  {
  }

  void
  Dump ()
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db,
                        "SELECT device_name, seq_no, action, filename, version, file_hash, file_seg_num, parent_device_name, parent_seq_no "
                        "   FROM ActionLog "
                        "   ORDER BY action_timestamp", -1, &stmt, 0);

    cout.setf(std::ios::left, std::ios::adjustfield);
    cout << ">> ACTION LOG <<" << endl;
    cout << "=============================================================================================================================================================================" << endl;
    cout << setw(30) << "device_name" << " | seq_no | action |" << setw(40) << " filename " << "  | version | file_hash  | seg_num | parent_device_name             | parent_seq_no" << endl;
    cout << "=============================================================================================================================================================================" << endl;

    while (sqlite3_step (stmt) == SQLITE_ROW)
      {
        cout << setw (30) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 0), sqlite3_column_bytes (stmt, 0))) << " | "; // device_name
        cout << setw (6) << sqlite3_column_int64 (stmt, 1) << " | "; // seq_no
        cout << setw (6) << (sqlite3_column_int   (stmt, 2)==0?"UPDATE":"DELETE") << " | "; // action
        cout << setw (40) << sqlite3_column_text  (stmt, 3) << " | "; // filename
        cout << setw (7) << sqlite3_column_int64 (stmt, 4) << " | "; // version

        if (sqlite3_column_int   (stmt, 2) == 0)
          {
            cout << setw (10) << Hash (sqlite3_column_blob  (stmt, 5), sqlite3_column_bytes (stmt, 5)).shortHash () << " | ";
            cout << setw (7) << sqlite3_column_int64 (stmt, 6) << " | "; // seg_num
          }
        else
          cout << "           |         | ";

        if (sqlite3_column_bytes (stmt, 7) > 0)
          {
            cout << setw (30) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 7), sqlite3_column_bytes (stmt, 7))) << " | "; // parent_device_name
            cout << setw (5) << sqlite3_column_int64 (stmt, 8); // seq_no
          }
        else
          cout << setw (30) << " " << " | ";
        cout << endl;
      }

    sqlite3_finalize (stmt);
  }

  void
  DumpActionData(const Ndnx::Name &deviceName, int64_t seqno)
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db, "SELECT action_content_object, action_name FROM ActionLog WHERE device_name = ? and seq_no = ?", -1, &stmt, 0);
    Ndnx::NdnxCharbufPtr device_name = deviceName.toNdnxCharbuf();
    sqlite3_bind_blob (stmt, 1, device_name->buf(), device_name->length(), SQLITE_STATIC);
    sqlite3_bind_int64 (stmt, 2, seqno);
    cout << "Dumping action data for: [" << deviceName << ", " << seqno << "]" <<endl;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
      PcoPtr pco = make_shared<ParsedContentObject> (reinterpret_cast<const unsigned char *> (sqlite3_column_blob (stmt, 0)), sqlite3_column_bytes (stmt, 0));
      Ndnx::Name actionName = Ndnx::Name(sqlite3_column_blob(stmt, 1), sqlite3_column_bytes(stmt, 0));
      if (pco)
      {
        ActionItemPtr action = deserializeMsg<ActionItem> (pco->content());
        if (action)
        {
          cout << "Action data size : " << pco->content().size() << endl;
          cout << "Action data name : " << actionName << endl;
          string type = action->action() == ActionItem::UPDATE ? "UPDATE" : "DELETE";
          cout << "Action Type = " << type << endl;
          cout << "Timestamp = " << action->timestamp() << endl;
          string filename = action->filename();
          cout << "Filename = " << filename << endl;
          if (action->has_seg_num())
          {
            cout << "Segment number = " << action->seg_num() << endl;
          }
          if (action->has_file_hash())
          {
            cout << "File hash = " << Hash(action->file_hash().c_str(), action->file_hash().size()) << endl;
          }
        }
        else
        {
          cerr << "Error! Failed to parse action from pco! " << endl;
        }
      }
      else
      {
        cerr << "Error! Invalid pco! " << endl;
      }
    }
    else
    {
      cerr << "Error! Can not find the requested action" << endl;
    }
    sqlite3_finalize(stmt);
  }
};

class FileStateDumper : public DbHelper
{
public:
  FileStateDumper (const filesystem::path &path)
  : DbHelper (path / ".chronoshare", "file-state.db")
  {
  }

  void
  Dump ()
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db,
                        "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num,directory,is_complete "
                        "   FROM FileState "
                        "   WHERE type = 0 ORDER BY filename", -1, &stmt, 0);

    cout.setf(std::ios::left, std::ios::adjustfield);
    cout << ">> FILE STATE <<" << endl;
    cout << "===================================================================================================================================" << endl;
    cout << "filename                                 | device_name                    | seq_no | file_hash  | seg_num | directory | is_complete" << endl;
    cout << "===================================================================================================================================" << endl;

    while (sqlite3_step (stmt) == SQLITE_ROW)
      {
        cout << setw (40) << sqlite3_column_text  (stmt, 0) << " | ";
        cout << setw (30) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1))) << " | ";
        cout << setw (6) << sqlite3_column_int64 (stmt, 2) << " | ";
        cout << setw (10) << Hash (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3)).shortHash () << " | ";
        cout << setw (6) << sqlite3_column_int64 (stmt, 6) << " | ";
        if (sqlite3_column_bytes (stmt, 7) == 0)
          cout << setw (20) << "<NULL>" << " | ";
        else
          cout << setw (20) << sqlite3_column_text (stmt, 7) << " | ";

        if (sqlite3_column_int (stmt, 8) == 0)
          cout << setw (20) << "no" << endl;
        else
          cout << setw (20) << "yes" << endl;
      }

    sqlite3_finalize (stmt);
  }
};

int main(int argc, char *argv[])
{
  INIT_LOGGERS ();

  if (argc != 3 && !(argc == 5 && string(argv[1]) == "action"))
    {
      cerr << "Usage: ./dump-db state|action|file|all <path-to-shared-folder>" << endl;
      cerr << "   or: ./dump-db action <path-to-shared-folder> <device-name> <seq-no>" << endl;
      return 1;
    }

  string type = argv[1];
  if (type == "state")
    {
      StateLogDumper dumper (argv[2]);
      dumper.DumpState ();
      dumper.DumpLog ();
    }
  else if (type == "action")
    {
      ActionLogDumper dumper (argv[2]);
      if (argc == 5)
      {
        dumper.DumpActionData(string(argv[3]), boost::lexical_cast<int64_t>(argv[4]));
      }
      else
      {
        dumper.Dump ();
      }
    }
  else if (type == "file")
    {
      FileStateDumper dumper (argv[2]);
      dumper.Dump ();
    }
  else if (type == "all")
    {
      {
        StateLogDumper dumper (argv[2]);
        dumper.DumpState ();
        dumper.DumpLog ();
      }

      {
        ActionLogDumper dumper (argv[2]);
        dumper.Dump ();
      }

      {
        FileStateDumper dumper (argv[2]);
        dumper.Dump ();
      }
    }
  else
    {
      cerr << "ERROR: Wrong database type" << endl;
      cerr << "\nUsage: ./dump-db state|action|file|all <path-to-shared-folder>" << endl;
      return 1;
    }


  return 0;
}
