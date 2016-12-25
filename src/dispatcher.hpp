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

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "action-log.hpp"
#include "ccnx-wrapper.hpp"
#include "content-server.hpp"
#include "executor.hpp"
#include "fetch-manager.hpp"
#include "object-db.hpp"
#include "object-manager.hpp"
#include "state-server.hpp"
#include "sync-core.hpp"

#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

typedef boost::shared_ptr<ActionItem> ActionItemPtr;

// TODO:
// This class lacks a permanent table to store the files in fetching process
// and fetch the missing pieces for those in the table after the application launches
class Dispatcher
{
public:
  // sharedFolder is the name to be used in NDN name;
  // rootDir is the shared folder dir in local file system;
  Dispatcher(const std::string& localUserName, const std::string& sharedFolder,
             const boost::filesystem::path& rootDir, Ccnx::CcnxWrapperPtr ccnx,
             bool enablePrefixDiscovery = true);
  ~Dispatcher();

  // ----- Callbacks, they only submit the job to executor and immediately return so that event processing thread won't be blocked for too long -------


  // callback to process local file change
  void
  Did_LocalFile_AddOrModify(const boost::filesystem::path& relativeFilepath);

  void
  Did_LocalFile_Delete(const boost::filesystem::path& relativeFilepath);

  /**
   * @brief Invoked when FileState is detected to have a file which does not exist on a file system
   */
  void
  Restore_LocalFile(FileItemPtr file);

  // for test
  HashPtr
  SyncRoot()
  {
    return m_core->root();
  }

  inline void
  LookupRecentFileActions(const boost::function<void(const std::string&, int, int)>& visitor,
                          int limit)
  {
    m_actionLog->LookupRecentFileActions(visitor, limit);
  }

private:
  void
  Did_LocalFile_AddOrModify_Execute(
    boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault

  void
  Did_LocalFile_Delete_Execute(
    boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault

  void
  Restore_LocalFile_Execute(FileItemPtr file);

private:
  /**
   * Callbacks:
   *
 x * - from SyncLog: when state changes -> to fetch missing actions
   *
 x * - from FetchManager/Actions: when action is fetched -> to request a file, specified by the action
   *                                                     -> to add action to the action log
   *
   * - from ActionLog/Delete:      when action applied (file state changed, file deleted)           -> to delete local file
   *
   * - from ActionLog/AddOrUpdate: when action applied (file state changes, file added or modified) -> to assemble the file if file is available in the ObjectDb, otherwise, do nothing
   *
 x * - from FetchManager/Files: when file segment is retrieved -> save it in ObjectDb
   *                            when file fetch is completed   -> if file belongs to FileState, then assemble it to filesystem. Don't do anything otherwise
   */

  // callback to process remote sync state change
  void
  Did_SyncLog_StateChange(SyncStateMsgPtr stateMsg);

  void
  Did_SyncLog_StateChange_Execute(SyncStateMsgPtr stateMsg);

  void
  Did_FetchManager_ActionFetch(const Ccnx::Name& deviceName, const Ccnx::Name& actionName,
                               uint32_t seqno, Ccnx::PcoPtr actionPco);

  void
  Did_ActionLog_ActionApply_Delete(const std::string& filename);

  void
  Did_ActionLog_ActionApply_Delete_Execute(std::string filename);

  // void
  // Did_ActionLog_ActionApply_AddOrModify (const std::string &filename, Ndnx::Name device_name, sqlite3_int64 seq_no,
  //                                        HashPtr hash, time_t m_time, int mode, int seg_num);

  void
  Did_FetchManager_FileSegmentFetch(const Ccnx::Name& deviceName, const Ccnx::Name& fileSegmentName,
                                    uint32_t segment, Ccnx::PcoPtr fileSegmentPco);

  void
  Did_FetchManager_FileSegmentFetch_Execute(Ccnx::Name deviceName, Ccnx::Name fileSegmentName,
                                            uint32_t segment, Ccnx::PcoPtr fileSegmentPco);

  void
  Did_FetchManager_FileFetchComplete(const Ccnx::Name& deviceName, const Ccnx::Name& fileBaseName);

  void
  Did_FetchManager_FileFetchComplete_Execute(Ccnx::Name deviceName, Ccnx::Name fileBaseName);

  void
  Did_LocalPrefix_Updated(const Ccnx::Name& prefix);

private:
  void
  AssembleFile_Execute(const Ccnx::Name& deviceName, const Hash& filehash,
                       const boost::filesystem::path& relativeFilepath);

  // void
  // fileChanged(const boost::filesystem::path &relativeFilepath, ActionType type);

  // void
  // syncStateChanged(const SyncStateMsgPtr &stateMsg);

  // void
  // actionReceived(const ActionItemPtr &actionItem);

  // void
  // fileSegmentReceived(const Ndnx::Name &name, const Ndnx::Bytes &content);

  // void
  // fileReady(const Ndnx::Name &fileNamePrefix);

private:
  Ccnx::CcnxWrapperPtr m_ccnx;
  SyncCore* m_core;
  SyncLogPtr m_syncLog;
  ActionLogPtr m_actionLog;
  FileStatePtr m_fileState;
  FileStatePtr m_fileStateCow;

  boost::filesystem::path m_rootDir;
  Executor m_executor;
  ObjectManager m_objectManager;
  Ndnx::Name m_localUserName;
  // maintain object db ptrs so that we don't need to create them
  // for every fetched segment of a file

  std::map<Hash, ObjectDbPtr> m_objectDbMap;

  std::string m_sharedFolder;
  ContentServer* m_server;
  StateServer* m_stateServer;
  bool m_enablePrefixDiscovery;

  FetchManagerPtr m_actionFetcher;
  FetchManagerPtr m_fileFetcher;
};

namespace Error {
struct Dispatcher : virtual boost::exception, virtual std::exception
{
};
typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;
}

#endif // DISPATCHER_H
