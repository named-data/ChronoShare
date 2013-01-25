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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "action-log.h"
#include "sync-core.h"
#include "ccnx-wrapper.h"
#include "executor.h"
#include "object-db.h"
#include "object-manager.h"
#include "content-server.h"
#include "fetch-manager.h"

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
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
  Dispatcher(const std::string &localUserName
             , const std::string &sharedFolder
             , const boost::filesystem::path &rootDir
             , Ccnx::CcnxWrapperPtr ccnx
             , int poolSize = 2
             , bool enablePrefixDiscovery = true
             );
  ~Dispatcher();

  // ----- Callbacks, they only submit the job to executor and immediately return so that event processing thread won't be blocked for too long -------


  // callback to process local file change
  void
  Did_LocalFile_AddOrModify (const boost::filesystem::path &relativeFilepath);

  void
  Did_LocalFile_Delete (const boost::filesystem::path &relativeFilepath);

  // for test
  HashPtr
  SyncRoot() { return m_core->root(); }

private:
  void
  Did_LocalFile_AddOrModify_Execute (boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault

  void
  Did_LocalFile_Delete_Execute (boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault


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
  Did_SyncLog_StateChange (SyncStateMsgPtr stateMsg);

  void
  Did_SyncLog_StateChange_Execute (SyncStateMsgPtr stateMsg);

  void
  Did_FetchManager_ActionFetch (const Ccnx::Name &deviceName, const Ccnx::Name &actionName, uint32_t seqno, Ccnx::PcoPtr actionPco);

  void
  Did_ActionLog_ActionApply_Delete (const std::string &filename);

  void
  Did_ActionLog_ActionApply_Delete_Execute (std::string filename);

  // void
  // Did_ActionLog_ActionApply_AddOrModify (const std::string &filename, Ccnx::Name device_name, sqlite3_int64 seq_no,
  //                                        HashPtr hash, time_t m_time, int mode, int seg_num);

  void
  Did_FetchManager_FileSegmentFetch (const Ccnx::Name &deviceName, const Ccnx::Name &fileSegmentName, uint32_t segment, Ccnx::PcoPtr fileSegmentPco);

  void
  Did_FetchManager_FileSegmentFetch_Execute (Ccnx::Name deviceName, Ccnx::Name fileSegmentName, uint32_t segment, Ccnx::PcoPtr fileSegmentPco);

  void
  Did_FetchManager_FileFetchComplete (const Ccnx::Name &deviceName, const Ccnx::Name &fileBaseName);

  void
  Did_FetchManager_FileFetchComplete_Execute (Ccnx::Name deviceName, Ccnx::Name fileBaseName);

  void
  Did_LocalPrefix_Updated (const Ccnx::Name &prefix);

private:
  void
  AssembleFile_Execute (const Ccnx::Name &deviceName, const Hash &filehash, const boost::filesystem::path &relativeFilepath);

  // void
  // fileChanged(const boost::filesystem::path &relativeFilepath, ActionType type);

  // void
  // syncStateChanged(const SyncStateMsgPtr &stateMsg);

  // void
  // actionReceived(const ActionItemPtr &actionItem);

  // void
  // fileSegmentReceived(const Ccnx::Name &name, const Ccnx::Bytes &content);

  // void
  // fileReady(const Ccnx::Name &fileNamePrefix);

private:
  Ccnx::CcnxWrapperPtr m_ccnx;
  SyncCore *m_core;
  SyncLogPtr   m_syncLog;
  ActionLogPtr m_actionLog;

  boost::filesystem::path m_rootDir;
  Executor m_executor;
  ObjectManager m_objectManager;
  Ccnx::Name m_localUserName;
  // maintain object db ptrs so that we don't need to create them
  // for every fetched segment of a file

  std::map<Hash, ObjectDbPtr> m_objectDbMap;

  std::string m_sharedFolder;
  ContentServer *m_server;
  bool m_enablePrefixDiscovery;

  FetchManagerPtr m_actionFetcher;
  FetchManagerPtr m_fileFetcher;
};

namespace Error
{
  struct Dispatcher : virtual boost::exception, virtual std::exception {};
  typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;
}

#endif // DISPATCHER_H

