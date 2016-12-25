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

#ifndef STATE_SERVER_H
#define STATE_SERVER_H

#include "action-log.hpp"
#include "ccnx-wrapper.hpp"
#include "executor.hpp"
#include "object-db.hpp"
#include "object-manager.hpp"
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <map>
#include <set>

#include "../contrib/json_spirit/json_spirit_value.h"
#include "../contrib/json_spirit/json_spirit_writer_template.h"

#ifndef JSON_SPIRIT_VALUE_ENABLED
#error Please define JSON_SPIRIT_VALUE_ENABLED for the Value type to be enabled
#endif

/**
 * @brief Class serving state information from ChronoShare
 *
 * Eventually, the same info/actions can be made available via a global scope prefix
 *
 * Information available:
 *
 * For now serving only locally (using <PREFIX> = /localhost/<user's-device-name>/"chronoshare"/<FOLDER>/"info")
 *
 * - state: get list of SyncNodes, their sequence numbers, and forwarding hint (almost the same as RECOVERY interest)
 *
 *   <PREFIX_INFO>/"state"   (@todo: authentification code or authentication code should in addition somewhere)
 *
 * - action
 *
 *   Get list of actions for a folder (for all files under this folder)
 *
 *   <PREFIX_INFO>/"actions"/"folder"/<offset>   (all actions)
 *   or
 *   <PREFIX_INFO>/"actions"/"folder"/<one-component-relative-file-name>/<offset>
 *
 *   Actions are ordered in decreasing order (latest will go first).
 *
 *   Each data packet contains up to 100 actions.
 *
 *   TEMPORARILY LIMIT IS REDUCED TO 10 ! (for debug purposes)
 *   (may be even not temporarily...)
 *
 *   If more items are available, application data will specify URL for the next packet
 *
 *   Format of returned data (JSON):
 *   {
 *      "actions": [
 *      {
 *          "id": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          },
 *          "timestamp": "<ACTION-TIMESTAMP>",
 *          "filename": "<FILENAME>",
 *
 *          "action": "UPDATE | DELETE",
 *
 *          // only if update
 *          "update": {
 *              "hash": "<FILE-HASH>",
 *              "timestamp": "<FILE-TIMESTAMP>",
 *              "chmod": "<FILE-MODE>",
 *              "segNum": "<NUMBER-OF-SEGMENTS (~file size)>"
 *          },
 *
 *          // if parent_device_name is set
 *          "parentId": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          };
 *      },
 *
 *      // only if there are more actions available
 *      "more": "next segment number"
 *   }
 *
 *
 * - file
 *
 *   <PREFIX_INFO>/"files"/"folder"/<offset>   (full filestate)
 *   or
 *   <PREFIX_INFO>/"files"/"folder"/<one-component-relative-folder-name>/<offset>
 *
 *   Each Data packets contains a list of up to 100 files.
 *   If more items are available, application data will specify URL for the next packet
 *
 *   TEMPORARILY LIMIT IS REDUCED TO 10 ! (for debug purposes)
 *   (may be even not temporarily...)
 *
 *   Format of returned data (JSON):
 *   {
 *      "files": [
 *      {
 *          "filename": "<FILENAME>",
 *          "owner": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          },
 *
 *          "hash": "<FILE-HASH>",
 *          "timestamp": "<FILE-TIMESTAMP>",
 *          "chmod": "<FILE-MODE>",
 *          "segNum": "<NUMBER-OF-SEGMENTS (~file size)>"
 *      }, ...,
 *      ]
 *
 *      // only if there are more actions available
 *      "more": "next segment number"
 *   }
 *
 * Commands available:
 *
 * For now serving only locally (using <PREFIX_CMD> = /localhost/<user's-device-name>/"chronoshare"/<FOLDER>/"cmd")
 *
 * - restore version of the file
 *
 *   <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>
 *   or
 *   <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
 *
 * - clean state log
 *   (this may not need to be here, if we implement periodic cleaning)
 * - ? flatten action log (should be supported eventually, but not supported now)
 */
class StateServer
{
public:
  StateServer(Ccnx::CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
              const boost::filesystem::path& rootDir, const Ccnx::Name& userName,
              const std::string& sharedFolderName, const std::string& appName,
              ObjectManager& objectManager, int freshness = -1);
  ~StateServer();

private:
  void
  info_actions_folder(const Ccnx::Name& interest);

  void
  info_actions_file(const Ccnx::Name& interest);

  void
  info_actions_fileOrFolder_Execute(const Ccnx::Name& interest, bool isFolder = true);

  void
  info_files_folder(const Ccnx::Name& interest);

  void
  info_files_folder_Execute(const Ccnx::Name& interest);

  void
  cmd_restore_file(const Ccnx::Name& interest);

  void
  cmd_restore_file_Execute(const Ccnx::Name& interest);

private:
  void
  registerPrefixes();

  void
  deregisterPrefixes();

  static void
  formatActionJson(json_spirit::Array& actions, const Ccnx::Name& name, sqlite3_int64 seq_no,
                   const ActionItem& action);

  static void
  formatFilestateJson(json_spirit::Array& files, const FileItem& file);

private:
  Ndnx::NdnxWrapperPtr m_ndnx;
  ActionLogPtr m_actionLog;
  ObjectManager& m_objectManager;

  Ndnx::Name m_PREFIX_INFO;
  Ndnx::Name m_PREFIX_CMD;

  boost::filesystem::path m_rootDir;
  int m_freshness;

  Executor m_executor;

  Ccnx::Name m_userName;
  std::string m_sharedFolderName;
  std::string m_appName;
};
#endif // CONTENT_SERVER_H
