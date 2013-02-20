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
 * Author: Jared Lindblom <lindblom@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#include "fs-watcher.h"
#include "db-helper.h"
#include "logging.h"

#include <boost/bind.hpp>

#include <QDirIterator>
#include <QRegExp>

using namespace std;
using namespace boost;

INIT_LOGGER ("FsWatcher");

FsWatcher::FsWatcher (QString dirPath,
                      LocalFile_Change_Callback onChange, LocalFile_Change_Callback onDelete,
                      QObject* parent)
  : QObject(parent)
  , m_watcher (new QFileSystemWatcher())
  , m_scheduler (new Scheduler ())
  , m_dirPath (dirPath)
  , m_onChange (onChange)
  , m_onDelete (onDelete)
{
  _LOG_DEBUG ("Monitor dir: " << m_dirPath.toStdString ());
  // add main directory to monitor

  initFileStateDb();

  m_watcher->addPath (m_dirPath);

  // register signals (callback functions)
  connect (m_watcher, SIGNAL (directoryChanged (QString)), this, SLOT (DidDirectoryChanged (QString)));
  connect (m_watcher, SIGNAL (fileChanged (QString)),      this, SLOT (DidFileChanged (QString)));

  m_scheduler->start ();

  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind (&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, m_dirPath),
                                  "rescan-r-" + m_dirPath.toStdString ()); // only one task will be scheduled per directory

  Scheduler::scheduleOneTimeTask (m_scheduler, 0,
                                  bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, m_dirPath),
                                  "rescan-" +m_dirPath.toStdString ()); // only one task will be scheduled per directory
}

FsWatcher::~FsWatcher()
{
  m_scheduler->shutdown ();
  sqlite3_close(m_db);
}

void
FsWatcher::DidDirectoryChanged (QString dirPath)
{
  _LOG_DEBUG ("Triggered DirPath: " << dirPath.toStdString ());

  filesystem::path absPathTriggeredDir (dirPath.toStdString ());
  if (!filesystem::exists (filesystem::path (absPathTriggeredDir)))
    {
      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, dirPath),
                                      "r-" + dirPath.toStdString ()); // only one task will be scheduled per directory
    }
  else
    {
      // m_executor.execute (bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));
      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath),
                                      dirPath.toStdString ()); // only one task will be scheduled per directory

      // m_executor.execute (bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));
      Scheduler::scheduleOneTimeTask (m_scheduler, 300,
                                      bind (&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath),
                                      "rescan-"+dirPath.toStdString ()); // only one task will be scheduled per directory

      Scheduler::scheduleOneTimeTask (m_scheduler, 300,
                                      bind (&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, m_dirPath),
                                      "rescan-r-" + m_dirPath.toStdString ()); // only one task will be scheduled per directory
    }
}

void
FsWatcher::DidFileChanged (QString filePath)
{
  if (!filePath.startsWith (m_dirPath))
    {
      _LOG_ERROR ("Got notification about a file not from the monitored directory: " << filePath.toStdString());
      return;
    }
  QString absFilePath = filePath;

  filesystem::path absPathTriggeredFile (filePath.toStdString ());
  filePath.remove (0, m_dirPath.size ());

  filesystem::path triggeredFile (filePath.toStdString ());
  if (filesystem::exists (filesystem::path (absPathTriggeredFile)))
    {
      _LOG_DEBUG ("Triggered UPDATE of file:  " << triggeredFile.relative_path ().generic_string ());
      // m_onChange (triggeredFile.relative_path ());

      m_watcher->removePath (absFilePath);
      m_watcher->addPath (absFilePath);

      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (m_onChange, triggeredFile.relative_path ()),
                                      triggeredFile.relative_path ().string());
    }
  else
    {
      _LOG_DEBUG ("Triggered DELETE of file: " << triggeredFile.relative_path ().generic_string ());
      // m_onDelete (triggeredFile.relative_path ());

      m_watcher->removePath (absFilePath);

      deleteFile(triggeredFile.relative_path());
      Scheduler::scheduleOneTimeTask (m_scheduler, 0.5,
                                      bind (m_onDelete, triggeredFile.relative_path ()),
                                      "r-" + triggeredFile.relative_path ().string());
    }
}

void
FsWatcher::ScanDirectory_NotifyUpdates_Execute (QString dirPath)
{
  _LOG_TRACE (" >> ScanDirectory_NotifyUpdates_Execute");

  // exclude working only on last component, not the full path; iterating through all directories, even excluded from monitoring
  QRegExp exclude ("^(\\.|\\.\\.|\\.chronoshare|.*~|.*\\.swp)$");

  QDirIterator dirIterator (dirPath,
                            QDir::Dirs | QDir::Files | /*QDir::Hidden |*/ QDir::NoSymLinks | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories); // directory iterator (recursive)

  // iterate through directory recursively
  while (dirIterator.hasNext ())
    {
      dirIterator.next ();

      // Get FileInfo
      QFileInfo fileInfo = dirIterator.fileInfo ();

      QString name = fileInfo.fileName ();
      _LOG_DEBUG ("+++ Scanning: " <<  name.toStdString ());

      if (!exclude.exactMatch (name))
        {
          _LOG_DEBUG ("Not excluded file/dir: " << fileInfo.absoluteFilePath ().toStdString ());
          QString absFilePath = fileInfo.absoluteFilePath ();

          // _LOG_DEBUG ("Attempt to add path to watcher: " << absFilePath.toStdString ());
          m_watcher->removePath (absFilePath);
          m_watcher->addPath (absFilePath);

          if (fileInfo.isFile ())
            {
              QString relFile = absFilePath;
              relFile.remove (0, m_dirPath.size ());
              filesystem::path aFile (relFile.toStdString ());

              if (
                  //!m_fileState->LookupFile (aFile.relative_path ().generic_string ()) /* file does not exist there, but exists locally: added */)
                  !fileExists(aFile.relative_path())  /*file does not exist in db, but exists in fs: add */)
                {
                  addFile(aFile.relative_path());
                  DidFileChanged (absFilePath);
                }
            }
        }
      else
        {
          // _LOG_DEBUG ("Excluded file/dir: " << fileInfo.filePath ().toStdString ());
        }
    }
}


void
FsWatcher::ScanDirectory_NotifyRemovals_Execute (QString dirPath)
{
  _LOG_DEBUG ("Triggered DirPath: " << dirPath.toStdString ());

  filesystem::path absPathTriggeredDir (dirPath.toStdString ());
  dirPath.remove (0, m_dirPath.size ());

  filesystem::path triggeredDir (dirPath.toStdString ());

  /*
  FileItemsPtr files = m_fileState->LookupFilesInFolderRecursively (triggeredDir.relative_path ().generic_string ());
  for (std::list<FileItem>::iterator file = files->begin (); file != files->end (); file ++)
    {
      filesystem::path testFile = filesystem::path (m_dirPath.toStdString ()) / file->filename ();
      _LOG_DEBUG ("Check file for deletion [" << testFile.generic_string () << "]");

      if (!filesystem::exists (testFile))
        {
          if (removeIncomplete || file->is_complete ())
            {
              _LOG_DEBUG ("Notifying about removed file [" << file->filename () << "]");
              m_onDelete (file->filename ());
            }
        }
    }
    */

  vector<string> files;
  getFilesInDir(triggeredDir.relative_path(), files);
  for (vector<string>::iterator file = files.begin(); file != files.end(); file++)
  {
    filesystem::path targetFile = filesystem::path (m_dirPath.toStdString()) / *file;
    if (!filesystem::exists (targetFile))
    {
      deleteFile(*file);
      m_onDelete(*file);
    }
  }
}

const string INIT_DATABASE = "\
CREATE TABLE IF NOT EXISTS                                      \n\
    Files(                                                      \n\
    filename      TEXT NOT NULL,                                \n\
    PRIMARY KEY (filename)                                      \n\
);                                                              \n\
CREATE INDEX filename_index ON Files (filename);                \n\
";

void
FsWatcher::initFileStateDb()
{
  filesystem::path dbFolder = filesystem::path (m_dirPath.toStdString()) / ".chronoshare" / "fs_watcher";
  filesystem::create_directories(dbFolder);

  int res = sqlite3_open((dbFolder / "filestate.db").string ().c_str (), &m_db);
  if (res != SQLITE_OK)
  {
    BOOST_THROW_EXCEPTION(Error::Db() << errmsg_info_str("Cannot open database: " + (dbFolder / "filestate.db").string()));
  }

  char *errmsg = 0;
  res = sqlite3_exec(m_db, INIT_DATABASE.c_str(), NULL, NULL, &errmsg);
  if (res != SQLITE_OK && errmsg != 0)
  {
      // _LOG_TRACE ("Init \"error\": " << errmsg);
      cout << "FS-Watcher DB error: " << errmsg << endl;
      sqlite3_free (errmsg);
  }
}

bool
FsWatcher::fileExists(const filesystem::path &filename)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(m_db, "SELECT * FROM Files WHERE filename = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  bool retval = false;
  if (sqlite3_step (stmt) == SQLITE_ROW)
  {
    retval = true;
  }
  sqlite3_finalize(stmt);

  return retval;
}

void
FsWatcher::addFile(const filesystem::path &filename)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO Files (filename) VALUES (?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void
FsWatcher::deleteFile(const filesystem::path &filename)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(m_db, "DELETE FROM Files WHERE filename = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void
FsWatcher::getFilesInDir(const filesystem::path &dir, vector<string> &files)
{
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(m_db, "SELECT * FROM Files WHERE filename LIKE ?;", -1, &stmt, 0);

  string dirStr = dir.string();
  ostringstream escapedDir;
  for (string::const_iterator ch = dirStr.begin (); ch != dirStr.end (); ch ++)
    {
      if (*ch == '%')
        escapedDir << "\\%";
      else
        escapedDir << *ch;
    }
  escapedDir << "/" << "%";
  string escapedDirStr = escapedDir.str ();
  sqlite3_bind_text (stmt, 1, escapedDirStr.c_str (), escapedDirStr.size (), SQLITE_STATIC);

  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    string filename(reinterpret_cast<const char *>(sqlite3_column_text (stmt, 0)), sqlite3_column_bytes(stmt, 0));
    files.push_back(filename);
  }

  sqlite3_finalize (stmt);

}

#if WAF
#include "fs-watcher.moc"
#include "fs-watcher.cc.moc"
#endif
