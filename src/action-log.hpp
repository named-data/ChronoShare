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

#ifndef CHRONOSHARE_SRC_ACTION_LOG_HPP
#define CHRONOSHARE_SRC_ACTION_LOG_HPP

#include "db-helper.hpp"
#include "file-state.hpp"
#include "sync-log.hpp"
#include "core/chronoshare-common.hpp"

#include "action-item.pb.h"
#include "file-item.pb.h"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

namespace ndn {
namespace chronoshare {

class ActionLog;
typedef shared_ptr<ActionLog> ActionLogPtr;
typedef shared_ptr<ActionItem> ActionItemPtr;

class ActionLog : public DbHelper
{
public:
  class Error : public DbHelper::Error
  {
  public:
    explicit Error(const std::string& what)
      : DbHelper::Error(what)
    {
    }
  };

public:
  typedef function<void(std::string /*filename*/, Name /*device_name*/, sqlite3_int64 /*seq_no*/,
                        ConstBufferPtr /*hash*/, time_t /*m_time*/, int /*mode*/, int /*seg_num*/)>
    OnFileAddedOrChangedCallback;

  typedef boost::function<void(std::string /*filename*/)> OnFileRemovedCallback;

public:
  ActionLog(Face& face, const boost::filesystem::path& path, SyncLogPtr syncLog,
            const std::string& sharedFolder, const name::Component& appName,
            OnFileAddedOrChangedCallback onFileAddedOrChanged, OnFileRemovedCallback onFileRemoved);

  virtual ~ActionLog()
  {
  }

  //////////////////////////
  // Local operations     //
  //////////////////////////
  ActionItemPtr
  AddLocalActionUpdate(const std::string& filename, const Buffer& hash, time_t wtime, int mode,
                       int seg_num);

  // void
  // AddActionMove(const std::string &oldFile, const std::string &newFile);

  ActionItemPtr
  AddLocalActionDelete(const std::string& filename);

  //////////////////////////
  // Remote operations    //
  //////////////////////////

  ActionItemPtr
  AddRemoteAction(const Name& deviceName, sqlite3_int64 seqno, shared_ptr<Data> actionData);

  /**
   * @brief Add remote action using just action's parsed content object
   *
   * This function extracts device name and sequence number from the content object's and calls the
   * overloaded method
   */
  ActionItemPtr
  AddRemoteAction(shared_ptr<Data> actionData);

  ///////////////////////////
  // General operations    //
  ///////////////////////////

  shared_ptr<Data>
  LookupActionData(const Name& deviceName, sqlite3_int64 seqno);

  shared_ptr<Data>
  LookupActionData(const Name& actionName);

  ActionItemPtr
  LookupAction(const Name& deviceName, sqlite3_int64 seqno);

  ActionItemPtr
  LookupAction(const Name& actionName);

  FileItemPtr
  LookupAction(const std::string& filename, sqlite3_int64 version, const Buffer& filehash);

  /**
   * @brief Lookup up to [limit] actions starting [offset] in decreasing order(by timestamp) and
   * calling visitor(device_name,seqno,action) for each action
   */
  bool
  LookupActionsInFolderRecursively(
    const function<void(const Name& name, sqlite3_int64 seq_no, const ActionItem&)>& visitor,
    const std::string& folder, int offset = 0, int limit = -1);

  bool
  LookupActionsForFile(
    const function<void(const Name& name, sqlite3_int64 seq_no, const ActionItem&)>& visitor,
    const std::string& file, int offset = 0, int limit = -1);

  void
  LookupRecentFileActions(const function<void(const std::string&, int, int)>& visitor, int limit = 5);

  //
  inline FileStatePtr
  GetFileState();

public:
  // for test purposes
  sqlite3_int64
  LogSize();

private:
  std::tuple<sqlite3_int64 /*version*/, BufferPtr /*device name*/, sqlite3_int64 /*seq_no*/>
  GetLatestActionForFile(const std::string& filename);

  static void
  apply_action_xFun(sqlite3_context* context, int argc, sqlite3_value** argv);

private:
  SyncLogPtr m_syncLog;
  FileStatePtr m_fileState;

  // Face& m_face;
  std::string m_sharedFolderName;
  name::Component m_appName;

  OnFileAddedOrChangedCallback m_onFileAddedOrChanged;
  OnFileRemovedCallback m_onFileRemoved;
  KeyChain m_keyChain;
};

inline FileStatePtr
ActionLog::GetFileState()
{
  return m_fileState;
}

} // namespace chronoshare
} // namespace ndn

#endif // CHRONOSHARE_SRC_ACTION_LOG_HPP
