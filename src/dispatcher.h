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
  typedef enum
  {
    UPDATE = 0,
    DELETE = 1
  } ActionType;

  // sharedFolder is the name to be used in NDN name;
  // rootDir is the shared folder dir in local file system;
  Dispatcher(const boost::filesystem::path &path, const std::string &localUserName,  const Ccnx::Name &localPrefix, const std::string &sharedFolder, const boost::filesystem::path &rootDir, const Ccnx::CcnxWrapperPtr &ccnx, const SchedulerPtr &scheduler, int poolSize = 2);
  ~Dispatcher();

  // ----- Callbacks, they only submit the job to executor and immediately return so that event processing thread won't be blocked for too long -------

  // callback to process local file change
  void
  fileChangedCallback(const boost::filesystem::path &relativeFilepath, ActionType type);

  // callback to process remote sync state change
  void
  syncStateChangedCallback(const SyncStateMsgPtr &stateMsg);

  // callback to process remote action data
  void
  actionReceivedCallback(const ActionItemPtr &actionItem);

  // callback to porcess file data
  void
  fileSegmentReceivedCallback(const Ccnx::Name &name, const Ccnx::Bytes &content);

  // callback to assemble file
  void
  fileReadyCallback(const Ccnx::Name &fileNamePrefix);

private:
  void
  fileChanged(const boost::filesystem::path &relativeFilepath, ActionType type);

  void
  syncStateChanged(const SyncStateMsgPtr &stateMsg);

  void
  actionReceived(const ActionItemPtr &actionItem);

  void
  fileSegmentReceived(const Ccnx::Name &name, const Ccnx::Bytes &content);

  void
  fileReady(const Ccnx::Name &fileNamePrefix);

private:
  SyncCore *m_core;
  ActionLogPtr m_log;
  CcnxWrapperPtr m_ccnx;
  boost::filesystem::path m_rootDir;
  Executor m_executor;
  ObjectManager m_objectManager;
  Ccnx::Name m_localUserName;
  // maintain object db ptrs so that we don't need to create them
  // for every fetched segment of a file
  map<Ccnx::Name, ObjectDbPtr> m_objectDbMap;
  std::string m_sharedFolder;
};

namespace Error
{
  struct Dispatcher : virtual boost::exception, virtual std::exception {};
  typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;
}

#endif // DISPATCHER_H

