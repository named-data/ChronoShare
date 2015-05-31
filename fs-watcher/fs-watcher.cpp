/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
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

#include "fs-watcher.hpp"
#include "core/logging.hpp"

#include <QDirIterator>
#include <QRegExp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(FsWatcher);

namespace fs = boost::filesystem;

FsWatcher::FsWatcher(boost::asio::io_service& io, QString dirPath, LocalFile_Change_Callback onChange,
                     LocalFile_Change_Callback onDelete, QObject* parent)
  : QObject(parent)
  , m_watcher(new QFileSystemWatcher(this))
  , m_scheduler(io)
  , m_dirPath(dirPath)
  , m_onChange(onChange)
  , m_onDelete(onDelete)
{
  _LOG_DEBUG("Monitor dir: " << m_dirPath.toStdString());
  // add main directory to monitor

  initFileStateDb();

  m_watcher->addPath(m_dirPath);

  // register signals(callback functions)
  connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(DidDirectoryChanged(QString)));
  connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(DidFileChanged(QString)));

  rescheduleEvent("rescan", m_dirPath.toStdString(), time::seconds(0),
                  bind(&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, m_dirPath));

  rescheduleEvent("rescan-r", m_dirPath.toStdString(), time::seconds(0),
                  bind(&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, m_dirPath));
}

FsWatcher::~FsWatcher()
{
  sqlite3_close(m_db);
}

void
FsWatcher::rescheduleEvent(const std::string& eventType, const std::string& path,
                           const time::milliseconds& period, const Scheduler::Event& callback)
{
  util::scheduler::ScopedEventId event(m_scheduler);
  event = m_scheduler.scheduleEvent(period, callback);

  // only one task per directory/file
  std::string key = eventType + ":" + path;
  m_events.erase(key);
  m_events.insert(std::make_pair(key, std::move(event)));
}

void
FsWatcher::DidDirectoryChanged(QString dirPath)
{
  _LOG_DEBUG("Triggered DirPath: " << dirPath.toStdString());

  fs::path absPathTriggeredDir(dirPath.toStdString());
  if (!fs::exists(fs::path(absPathTriggeredDir))) {
    rescheduleEvent("r-", dirPath.toStdString(), time::milliseconds(500),
                    bind(&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, dirPath));
  }
  else {
    rescheduleEvent("", dirPath.toStdString(), time::milliseconds(500),
                    bind(&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));

    // rescan updated folder for updates
    rescheduleEvent("rescan", dirPath.toStdString(), time::seconds(300),
                    bind(&FsWatcher::ScanDirectory_NotifyUpdates_Execute, this, dirPath));

    // rescan whole folder for deletions
    rescheduleEvent("rescan-r", m_dirPath.toStdString(), time::seconds(300),
                    bind(&FsWatcher::ScanDirectory_NotifyRemovals_Execute, this, m_dirPath));
  }
}

void
FsWatcher::DidFileChanged(QString filePath)
{
  if (!filePath.startsWith(m_dirPath)) {
    _LOG_ERROR(
      "Got notification about a file not from the monitored directory: " << filePath.toStdString());
    return;
  }
  QString absFilePath = filePath;

  fs::path absPathTriggeredFile(filePath.toStdString());
  filePath.remove(0, m_dirPath.size());

  fs::path triggeredFile(filePath.toStdString());
  if (fs::exists(fs::path(absPathTriggeredFile))) {
    _LOG_DEBUG("Triggered UPDATE of file:  " << triggeredFile.relative_path().generic_string());
    // m_onChange(triggeredFile.relative_path());

    m_watcher->removePath(absFilePath);
    m_watcher->addPath(absFilePath);

    rescheduleEvent("", triggeredFile.relative_path().string(), time::milliseconds(500),
                    bind(m_onChange, triggeredFile.relative_path()));
  }
  else {
    _LOG_DEBUG("Triggered DELETE of file: " << triggeredFile.relative_path().generic_string());
    // m_onDelete(triggeredFile.relative_path());

    m_watcher->removePath(absFilePath);

    deleteFile(triggeredFile.relative_path());

    rescheduleEvent("r", triggeredFile.relative_path().string(), time::milliseconds(500),
                    bind(m_onDelete, triggeredFile.relative_path()));
  }
}

void
FsWatcher::ScanDirectory_NotifyUpdates_Execute(QString dirPath)
{
  _LOG_TRACE(" >> ScanDirectory_NotifyUpdates_Execute");

  // exclude working only on last component, not the full path; iterating through all directories,
  // even excluded from monitoring
  QRegExp exclude("^(\\.|\\.\\.|\\.chronoshare|.*~|.*\\.swp)$");

  QDirIterator dirIterator(dirPath, QDir::Dirs | QDir::Files | /*QDir::Hidden |*/ QDir::NoSymLinks |
                                      QDir::NoDotAndDotDot,
                           QDirIterator::Subdirectories); // directory iterator(recursive)

  // iterate through directory recursively
  while (dirIterator.hasNext()) {
    dirIterator.next();

    // Get FileInfo
    QFileInfo fileInfo = dirIterator.fileInfo();

    QString name = fileInfo.fileName();
    _LOG_DEBUG("+++ Scanning: " << name.toStdString());

    if (!exclude.exactMatch(name)) {
      _LOG_DEBUG("Not excluded file/dir: " << fileInfo.absoluteFilePath().toStdString());
      QString absFilePath = fileInfo.absoluteFilePath();

      // _LOG_DEBUG("Attempt to add path to watcher: " << absFilePath.toStdString());
      m_watcher->removePath(absFilePath);
      m_watcher->addPath(absFilePath);

      if (fileInfo.isFile()) {
        QString relFile = absFilePath;
        relFile.remove(0, m_dirPath.size());
        fs::path aFile(relFile.toStdString());

        if (
          //!m_fileState->LookupFile(aFile.relative_path().generic_string())
          ///* file does not exist there, but exists locally: added */)
          !fileExists(aFile.relative_path()) /*file does not exist in db, but exists in fs: add */) {
          addFile(aFile.relative_path());
          DidFileChanged(absFilePath);
        }
      }
    }
    else {
      // _LOG_DEBUG("Excluded file/dir: " << fileInfo.filePath().toStdString());
    }
  }
}


void
FsWatcher::ScanDirectory_NotifyRemovals_Execute(QString dirPath)
{
  _LOG_DEBUG("Triggered DirPath: " << dirPath.toStdString());

  fs::path absPathTriggeredDir(dirPath.toStdString());
  dirPath.remove(0, m_dirPath.size());

  fs::path triggeredDir(dirPath.toStdString());

  /*
  FileItemsPtr files = m_fileState->LookupFilesInFolderRecursively(triggeredDir.relative_path().generic_string());
  for (std::list<FileItem>::iterator file = files->begin(); file != files->end(); file ++)
    {
      fs::path testFile = fs::path(m_dirPath.toStdString()) / file->filename();
      _LOG_DEBUG("Check file for deletion [" << testFile.generic_string() << "]");

      if (!fs::exists(testFile))
        {
          if (removeIncomplete || file->is_complete())
            {
              _LOG_DEBUG("Notifying about removed file [" << file->filename() << "]");
              m_onDelete(file->filename());
            }
        }
    }
    */

  std::vector<std::string> files;
  getFilesInDir(triggeredDir.relative_path(), files);
  for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); file++) {
    fs::path targetFile = fs::path(m_dirPath.toStdString()) / *file;
    if (!fs::exists(targetFile)) {
      deleteFile(*file);
      m_onDelete(*file);
    }
  }
}

const std::string INIT_DATABASE = "\
CREATE TABLE IF NOT EXISTS                                     \n\
    Files(                                                     \n\
    filename      TEXT NOT NULL,                               \n\
    PRIMARY KEY(filename)                                      \n\
);                                                             \n\
CREATE INDEX IF NOT EXISTS filename_index ON Files(filename);  \n\
";

void
FsWatcher::initFileStateDb()
{
  fs::path dbFolder = fs::path(m_dirPath.toStdString()) / ".chronoshare" / "fs_watcher";
  fs::create_directories(dbFolder);

  int res = sqlite3_open((dbFolder / "filestate.db").string().c_str(), &m_db);
  if (res != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(Error("Cannot open database: " + (dbFolder / "filestate.db").string()));
  }

  char* errmsg = 0;
  res = sqlite3_exec(m_db, INIT_DATABASE.c_str(), NULL, NULL, &errmsg);
  if (res != SQLITE_OK && errmsg != 0) {
    // _LOG_TRACE("Init \"error\": " << errmsg);
    std::cout << "FS-Watcher DB error: " << errmsg << std::endl;
    sqlite3_free(errmsg);
  }
}

bool
FsWatcher::fileExists(const fs::path& filename)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "SELECT * FROM Files WHERE filename = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  bool retval = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    retval = true;
  }
  sqlite3_finalize(stmt);

  return retval;
}

void
FsWatcher::addFile(const fs::path& filename)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO Files(filename) VALUES(?);", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void
FsWatcher::deleteFile(const fs::path& filename)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "DELETE FROM Files WHERE filename = ?;", -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void
FsWatcher::getFilesInDir(const fs::path& dir, std::vector<std::string>& files)
{
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(m_db, "SELECT * FROM Files WHERE filename LIKE ?;", -1, &stmt, 0);

  std::string dirStr = dir.string();
  std::ostringstream escapedDir;
  for (std::string::const_iterator ch = dirStr.begin(); ch != dirStr.end(); ch++) {
    if (*ch == '%')
      escapedDir << "\\%";
    else
      escapedDir << *ch;
  }
  escapedDir << "/"
             << "%";
  std::string escapedDirStr = escapedDir.str();
  sqlite3_bind_text(stmt, 1, escapedDirStr.c_str(), escapedDirStr.size(), SQLITE_STATIC);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string filename(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                         sqlite3_column_bytes(stmt, 0));
    files.push_back(filename);
  }

  sqlite3_finalize(stmt);
}

} // namespace chronoshare
} // namespace ndn

#ifdef WAF
#include "fs-watcher.moc"
// #include "fs-watcher.cpp.moc"
#endif
