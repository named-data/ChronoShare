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

#include "state-server.h"
#include "logging.h"
#include "periodic-task.h"
#include "simple-interval-generator.h"
#include "task.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <utility>

INIT_LOGGER("StateServer")

using namespace Ndnx;
using namespace std;
using namespace boost;

StateServer::StateServer(CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
                         const boost::filesystem::path& rootDir, const Ccnx::Name& userName,
                         const std::string& sharedFolderName, const std::string& appName,
                         ObjectManager& objectManager, int freshness /* = -1*/)
  : m_ccnx(ccnx)
  , m_actionLog(actionLog)
  , m_objectManager(objectManager)
  , m_rootDir(rootDir)
  , m_freshness(freshness)
  , m_executor(1)
  , m_userName(userName)
  , m_sharedFolderName(sharedFolderName)
  , m_appName(appName)
{
  // may be later /localhost should be replaced with /%C1.M.S.localhost

  // <PREFIX_INFO> = /localhost/<user's-device-name>/"chronoshare"/"info"
  m_PREFIX_INFO = Name("/localhost")(m_userName)("chronoshare")(m_sharedFolderName)("info");

  // <PREFIX_CMD> = /localhost/<user's-device-name>/"chronoshare"/"cmd"
  m_PREFIX_CMD = Name("/localhost")(m_userName)("chronoshare")(m_sharedFolderName)("cmd");

  m_executor.start();

  registerPrefixes();
}

StateServer::~StateServer()
{
  m_executor.shutdown();

  deregisterPrefixes();
}

void
StateServer::registerPrefixes()
{
  // currently supporting limited number of command.
  // will be extended to support all planned commands later

  // <PREFIX_INFO>/"actions"/"all"/<segment>  get list of all actions
  m_ccnx->setInterestFilter(Name(m_PREFIX_INFO)("actions")("folder"),
                            bind(&StateServer::info_actions_folder, this, _1));
  m_ccnx->setInterestFilter(Name(m_PREFIX_INFO)("actions")("file"),
                            bind(&StateServer::info_actions_file, this, _1));

  // <PREFIX_INFO>/"filestate"/"all"/<segment>
  m_ccnx->setInterestFilter(Name(m_PREFIX_INFO)("files")("folder"),
                            bind(&StateServer::info_files_folder, this, _1));

  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
  m_ccnx->setInterestFilter(Name(m_PREFIX_CMD)("restore")("file"),
                            bind(&StateServer::cmd_restore_file, this, _1));
}

void
StateServer::deregisterPrefixes()
{
  m_ccnx->clearInterestFilter(Name(m_PREFIX_INFO)("actions")("folder"));
  m_ccnx->clearInterestFilter(Name(m_PREFIX_INFO)("actions")("file"));
  m_ccnx->clearInterestFilter(Name(m_PREFIX_INFO)("files")("folder"));
  m_ccnx->clearInterestFilter(Name(m_PREFIX_CMD)("restore")("file"));
}

void
StateServer::formatActionJson(json_spirit::Array& actions, const Ccnx::Name& name,
                              sqlite3_int64 seq_no, const ActionItem& action)
{
  /*
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
 *          }
 *      }
 */

  using namespace json_spirit;
  using namespace boost::posix_time;

  Object json;
  Object id;

  id.push_back(Pair("userName", boost::lexical_cast<string>(name)));
  id.push_back(Pair("seqNo", static_cast<int64_t>(seq_no)));

  json.push_back(Pair("id", id));

  json.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(action.timestamp()))));
  json.push_back(Pair("filename", action.filename()));
  json.push_back(Pair("version", action.version()));
  json.push_back(Pair("action", (action.action() == 0) ? "UPDATE" : "DELETE"));

  if (action.action() == 0) {
    Object update;
    update.push_back(Pair("hash", boost::lexical_cast<string>(
                                    Hash(action.file_hash().c_str(), action.file_hash().size()))));
    update.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(action.mtime()))));

    ostringstream chmod;
    chmod << setbase(8) << setfill('0') << setw(4) << action.mode();
    update.push_back(Pair("chmod", chmod.str()));

    update.push_back(Pair("segNum", action.seg_num()));
    json.push_back(Pair("update", update));
  }

  if (action.has_parent_device_name()) {
    Object parentId;
    Ccnx::Name parent_device_name(action.parent_device_name().c_str(),
                                  action.parent_device_name().size());
    id.push_back(Pair("userName", boost::lexical_cast<string>(parent_device_name)));
    id.push_back(Pair("seqNo", action.parent_seq_no()));

    json.push_back(Pair("parentId", parentId));
  }

  actions.push_back(json);
}

void
StateServer::info_actions_folder(const Name& interest)
{
  if (interest.size() - m_PREFIX_INFO.size() != 3 && interest.size() - m_PREFIX_INFO.size() != 4) {
    _LOG_DEBUG("Invalid interest: " << interest);
    return;
  }

  _LOG_DEBUG(">> info_actions_folder: " << interest);
  m_executor.execute(bind(&StateServer::info_actions_fileOrFolder_Execute, this, interest, true));
}

void
StateServer::info_actions_file(const Name& interest)
{
  if (interest.size() - m_PREFIX_INFO.size() != 3 && interest.size() - m_PREFIX_INFO.size() != 4) {
    _LOG_DEBUG("Invalid interest: " << interest);
    return;
  }

  _LOG_DEBUG(">> info_actions_file: " << interest);
  m_executor.execute(bind(&StateServer::info_actions_fileOrFolder_Execute, this, interest, false));
}


void
StateServer::info_actions_fileOrFolder_Execute(const Ccnx::Name& interest, bool isFolder /* = true*/)
{
  // <PREFIX_INFO>/"actions"/"folder|file"/<folder|file>/<offset>  get list of all actions

  try {
    int offset = interest.getCompFromBackAsInt(0);

    /// @todo !!! add security checking

    string fileOrFolderName;
    if (interest.size() - m_PREFIX_INFO.size() == 4)
      fileOrFolderName = interest.getCompFromBackAsString(1);
    else // == 3
      fileOrFolderName = "";
    /*
 *   {
 *      "actions": [
 *           ...
 *      ],
 *
 *      // only if there are more actions available
 *      "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-ACTION>"
 *   }
 */

    using namespace json_spirit;
    Object json;

    Array actions;
    bool more;
    if (isFolder) {
      more =
        m_actionLog->LookupActionsInFolderRecursively(boost::bind(StateServer::formatActionJson,
                                                                  boost::ref(actions), _1, _2, _3),
                                                      fileOrFolderName, offset * 10, 10);
    }
    else {
      more = m_actionLog->LookupActionsForFile(boost::bind(StateServer::formatActionJson,
                                                           boost::ref(actions), _1, _2, _3),
                                               fileOrFolderName, offset * 10, 10);
    }

    json.push_back(Pair("actions", actions));

    if (more) {
      json.push_back(Pair("more", lexical_cast<string>(offset + 1)));
      // Ccnx::Name more = Name (interest.getPartialName (0, interest.size () - 1))(offset + 1);
      // json.push_back (Pair ("more", lexical_cast<string> (more)));
    }

    ostringstream os;
    write_stream(Value(json), os, pretty_print | raw_utf8);
    m_ccnx->publishData(interest, os.str(), 1);
  }
  catch (Ccnx::NameException& ne) {
    // ignore any unexpected interests and errors
    _LOG_ERROR(*boost::get_error_info<Ccnx::error_info_str>(ne));
  }
}

void
StateServer::formatFilestateJson(json_spirit::Array& files, const FileItem& file)
{
  /**
 *   {
 *      "filestate": [
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
 *      "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-FILESTATE>"
 *   }
 */
  using namespace json_spirit;
  using namespace boost::posix_time;

  Object json;

  json.push_back(Pair("filename", file.filename()));
  json.push_back(Pair("version", file.version()));
  {
    Object owner;
    Ccnx::Name device_name(file.device_name().c_str(), file.device_name().size());
    owner.push_back(Pair("userName", boost::lexical_cast<string>(device_name)));
    owner.push_back(Pair("seqNo", file.seq_no()));

    json.push_back(Pair("owner", owner));
  }

  json.push_back(Pair("hash", boost::lexical_cast<string>(
                                Hash(file.file_hash().c_str(), file.file_hash().size()))));
  json.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(file.mtime()))));

  ostringstream chmod;
  chmod << setbase(8) << setfill('0') << setw(4) << file.mode();
  json.push_back(Pair("chmod", chmod.str()));

  json.push_back(Pair("segNum", file.seg_num()));

  files.push_back(json);
}

void
debugFileState(const FileItem& file)
{
  std::cout << file.filename() << std::endl;
}

void
StateServer::info_files_folder(const Ccnx::Name& interest)
{
  if (interest.size() - m_PREFIX_INFO.size() != 3 && interest.size() - m_PREFIX_INFO.size() != 4) {
    _LOG_DEBUG("Invalid interest: " << interest << ", " << interest.size() - m_PREFIX_INFO.size());
    return;
  }

  _LOG_DEBUG(">> info_files_folder: " << interest);
  m_executor.execute(bind(&StateServer::info_files_folder_Execute, this, interest));
}


void
StateServer::info_files_folder_Execute(const Ccnx::Name& interest)
{
  // <PREFIX_INFO>/"filestate"/"folder"/<one-component-relative-folder-name>/<offset>
  try {
    int offset = interest.getCompFromBackAsInt(0);

    // /// @todo !!! add security checking

    string folder;
    if (interest.size() - m_PREFIX_INFO.size() == 4)
      folder = interest.getCompFromBackAsString(1);
    else // == 3
      folder = "";

    /*
 *   {
 *      "files": [
 *           ...
 *      ],
 *
 *      // only if there are more actions available
 *      "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-ACTION>"
 *   }
 */

    using namespace json_spirit;
    Object json;

    Array files;
    bool more = m_actionLog->GetFileState()
                  ->LookupFilesInFolderRecursively(boost::bind(StateServer::formatFilestateJson,
                                                               boost::ref(files), _1),
                                                   folder, offset * 10, 10);

    json.push_back(Pair("files", files));

    if (more) {
      json.push_back(Pair("more", lexical_cast<string>(offset + 1)));
      // Ccnx::Name more = Name (interest.getPartialName (0, interest.size () - 1))(offset + 1);
      // json.push_back (Pair ("more", lexical_cast<string> (more)));
    }

    ostringstream os;
    write_stream(Value(json), os, pretty_print | raw_utf8);
    m_ccnx->publishData(interest, os.str(), 1);
  }
  catch (Ccnx::NameException& ne) {
    // ignore any unexpected interests and errors
    _LOG_ERROR(*boost::get_error_info<Ccnx::error_info_str>(ne));
  }
}


void
StateServer::cmd_restore_file(const Ccnx::Name& interest)
{
  if (interest.size() - m_PREFIX_CMD.size() != 4 && interest.size() - m_PREFIX_CMD.size() != 5) {
    _LOG_DEBUG("Invalid interest: " << interest);
    return;
  }

  _LOG_DEBUG(">> cmd_restore_file: " << interest);
  m_executor.execute(bind(&StateServer::cmd_restore_file_Execute, this, interest));
}

void
StateServer::cmd_restore_file_Execute(const Ccnx::Name& interest)
{
  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>

  /// @todo !!! add security checking

  try {
    FileItemPtr file;

    if (interest.size() - m_PREFIX_CMD.size() == 5) {
      Hash hash(head(interest.getCompFromBack(0)), interest.getCompFromBack(0).size());
      int64_t version = interest.getCompFromBackAsInt(1);
      string filename =
        interest.getCompFromBackAsString(2); // should be safe even with full relative path

      file = m_actionLog->LookupAction(filename, version, hash);
      if (!file) {
        _LOG_ERROR("Requested file is not found: [" << filename << "] version [" << version
                                                    << "] hash ["
                                                    << hash.shortHash()
                                                    << "]");
      }
    }
    else {
      int64_t version = interest.getCompFromBackAsInt(0);
      string filename =
        interest.getCompFromBackAsString(1); // should be safe even with full relative path

      file = m_actionLog->LookupAction(filename, version, Hash(0, 0));
      if (!file) {
        _LOG_ERROR("Requested file is not found: [" << filename << "] version [" << version << "]");
      }
    }

    if (!file) {
      m_ccnx->publishData(interest, "FAIL: Requested file is not found", 1);
      return;
    }

    Hash hash = Hash(file->file_hash().c_str(), file->file_hash().size());

    ///////////////////
    // now the magic //
    ///////////////////

    boost::filesystem::path filePath = m_rootDir / file->filename();
    Name deviceName(file->device_name().c_str(), file->device_name().size());

    try {
      if (filesystem::exists(filePath) && filesystem::last_write_time(filePath) == file->mtime() &&
#if BOOST_VERSION >= 104900
          filesystem::status(filePath).permissions() == static_cast<filesystem::perms>(file->mode()) &&
#endif
          *Hash::FromFileContent(filePath) == hash) {
        m_ccnx->publishData(interest, "OK: File already exists", 1);
        _LOG_DEBUG("Asking to assemble a file, but file already exists on a filesystem");
        return;
      }
    }
    catch (filesystem::filesystem_error& error) {
      m_ccnx->publishData(interest, "FAIL: File operation failed", 1);
      _LOG_ERROR("File operations failed on [" << filePath << "] (ignoring)");
    }

    _LOG_TRACE("Restoring file [" << filePath << "]");
    if (m_objectManager.objectsToLocalFile(deviceName, hash, filePath)) {
      last_write_time(filePath, file->mtime());
#if BOOST_VERSION >= 104900
      permissions(filePath, static_cast<filesystem::perms>(file->mode()));
#endif
      m_ccnx->publishData(interest, "OK", 1);
    }
    else {
      m_ccnx->publishData(interest, "FAIL: Unknown error while restoring file", 1);
    }
  }
  catch (Ccnx::NameException& ne) {
    // ignore any unexpected interests and errors
    _LOG_ERROR(*boost::get_error_info<Ccnx::error_info_str>(ne));
  }
}
