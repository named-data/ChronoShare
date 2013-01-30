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
 */

#include "dispatcher.h"
#include "fs-watcher.h"
#include "logging.h"
#include "ccnx-wrapper.h"

#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace std;
using namespace Ccnx;

class ActionLogDumper : public DbHelper
{
public:
  ActionLogDumper (const filesystem::path &path)
  : DbHelper (path / ".chronoshare", "action-log.db")
  {
  }

  void
  DumpActionLog ()
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

    FileItemsPtr retval = make_shared<FileItems> ();
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
  DumpFileState ()
  {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2 (m_db,
                        "SELECT filename,device_name,seq_no,file_hash,strftime('%s', file_mtime),file_chmod,file_seg_num "
                        "   FROM FileState "
                        "   WHERE type = 0 ORDER BY filename", -1, &stmt, 0);

    cout.setf(std::ios::left, std::ios::adjustfield);
    cout << ">> FILE STATE <<" << endl;
    cout << "==============================================================================================================" << endl;
    cout << "filename                                 | device_name                    | seq_no | file_hash  | file_seg_num" << endl;
    cout << "==============================================================================================================" << endl;

    FileItemsPtr retval = make_shared<FileItems> ();
    while (sqlite3_step (stmt) == SQLITE_ROW)
      {
        cout << setw (40) << sqlite3_column_text  (stmt, 0) << " | ";
        cout << setw (30) << lexical_cast<string> (Name (sqlite3_column_blob  (stmt, 1), sqlite3_column_bytes (stmt, 1))) << " | ";
        cout << setw (6) << sqlite3_column_int64 (stmt, 2) << " | ";
        cout << setw (10) << Hash (sqlite3_column_blob  (stmt, 3), sqlite3_column_bytes (stmt, 3)).shortHash () << " | ";
        cout << setw (6) << sqlite3_column_int64 (stmt, 6) << endl;
      }

    sqlite3_finalize (stmt);
  }
};

int main(int argc, char *argv[])
{
  INIT_LOGGERS ();

  if (argc != 2)
    {
      cerr << "Usage: ./dump-db <path-to-shared-folder>" << endl;
      return 1;
    }

  ActionLogDumper dumper (argv[1]);
  dumper.DumpActionLog ();
  cout << endl;
  dumper.DumpFileState ();

  return 0;
}
