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
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#include "state-server.h"
#include "logging.h"
#include <boost/make_shared.hpp>
#include <utility>
#include "task.h"
#include "periodic-task.h"
#include "simple-interval-generator.h"
#include <boost/lexical_cast.hpp>

INIT_LOGGER ("StateServer");

using namespace Ccnx;
using namespace std;
using namespace boost;

StateServer::StateServer(CcnxWrapperPtr ccnx, ActionLogPtr actionLog,
                         const boost::filesystem::path &rootDir,
                         const Ccnx::Name &userName, const std::string &sharedFolderName,
                         const std::string &appName,
                         ObjectManager &objectManager,
                         int freshness/* = -1*/)
  : m_ccnx(ccnx)
  , m_actionLog(actionLog)
  , m_objectManager (objectManager)
  , m_rootDir(rootDir)
  , m_freshness(freshness)
  , m_executor (1)
  , m_userName (userName)
  , m_sharedFolderName (sharedFolderName)
  , m_appName (appName)
{
  // may be later /localhost should be replaced with /%C1.M.S.localhost

  // <PREFIX_INFO> = /localhost/<user's-device-name>/"chronoshare"/"info"
  m_PREFIX_INFO = Name ("/localhost")(m_userName)("chronoshare")(m_sharedFolderName)("info");

  // <PREFIX_CMD> = /localhost/<user's-device-name>/"chronoshare"/"cmd"
  m_PREFIX_CMD = Name ("/localhost")(m_userName)("chronoshare")(m_sharedFolderName)("cmd");

  m_executor.start ();

  registerPrefixes ();
}

StateServer::~StateServer()
{
  m_executor.shutdown ();

  deregisterPrefixes ();
}

void
StateServer::registerPrefixes ()
{
  // currently supporting limited number of command.
  // will be extended to support all planned commands later

  // <PREFIX_INFO>/"actions"/"all"/<nonce>/<segment>  get list of all actions
  m_ccnx->setInterestFilter (Name (m_PREFIX_INFO)("actions")("folder"), bind(&StateServer::info_actions_folder, this, _1));

  // <PREFIX_INFO>/"filestate"/"all"/<nonce>/<segment>
  m_ccnx->setInterestFilter (Name (m_PREFIX_INFO)("filestate")("folder"), bind(&StateServer::info_filestate_folder, this, _1));

  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
  m_ccnx->setInterestFilter (Name (m_PREFIX_CMD)("restore")("file"), bind(&StateServer::cmd_restore_file, this, _1));
}

void
StateServer::deregisterPrefixes ()
{
  m_ccnx->clearInterestFilter (Name (m_PREFIX_INFO)("actions")("folder"));
  m_ccnx->clearInterestFilter (Name (m_PREFIX_INFO)("filestate")("folder"));
  m_ccnx->clearInterestFilter (Name (m_PREFIX_CMD) ("restore")("file"));
}

// void
// StateServer::info_actions_all (const Name &interest)
// {
//   _LOG_DEBUG (">> info_actions_all: " << interest);
//   m_executor.execute (bind (&StateServer::info_actions_all_Execute, this, interest));
// }

// void
// StateServer::info_actions_all_Execute (const Name &interest)
// {
//   // <PREFIX_INFO>/"actions"/"all"/<nonce>/<offset>  get list of all actions

//   try
//     {
//       int offset = interest.getCompFromBackAsInt (0);

//       // LookupActionsInFolderRecursively
//       /// @todo !!! add security checking
//       m_ccnx->publishData (interest, "FAIL: Not implemented", 1);
//     }
//   catch (Ccnx::NameException &ne)
//     {
//       // ignore any unexpected interests and errors
//       _LOG_ERROR (*boost::get_error_info<Ccnx::error_info_str>(ne));
//     }
// }

void debugAction (const Ccnx::Name &name, sqlite3_int64 seq_no, const ActionItem &action)
{
  std::cout << name << ", " << seq_no << ", " << action.filename () << std::endl;
}

void
StateServer::info_actions_folder (const Name &interest)
{
  if (interest.size () - m_PREFIX_INFO.size () != 4 &&
      interest.size () - m_PREFIX_INFO.size () != 5)
    {
      _LOG_DEBUG ("Invalid interest: " << interest);
      return;
    }

  _LOG_DEBUG (">> info_actions_all: " << interest);
  m_executor.execute (bind (&StateServer::info_actions_folder_Execute, this, interest));
}

void
StateServer::info_actions_folder_Execute (const Name &interest)
{
  // <PREFIX_INFO>/"actions"/"folder"/<folder>/<nonce>/<offset>  get list of all actions

  try
    {
      int offset = interest.getCompFromBackAsInt (0);

      /// @todo !!! add security checking

      string folder;
      if (interest.size () - m_PREFIX_INFO.size () == 5)
        folder = interest.getCompFromBackAsString (2);
      else // == 4
        folder = "";

      m_actionLog->LookupActionsInFolderRecursively (debugAction, folder, offset*100, 100);

      // m_ccnx->publishData (interest, "FAIL: Not implemented", 1);
    }
  catch (Ccnx::NameException &ne)
    {
      // ignore any unexpected interests and errors
      _LOG_ERROR (*boost::get_error_info<Ccnx::error_info_str>(ne));
    }
}

void debugFileState (const FileItem &file)
{
  std::cout << file.filename () << std::endl;
}

void
StateServer::info_filestate_folder (const Ccnx::Name &interest)
{
  if (interest.size () - m_PREFIX_INFO.size () != 4 &&
      interest.size () - m_PREFIX_INFO.size () != 5)
    {
      _LOG_DEBUG ("Invalid interest: " << interest << ", " << interest.size () - m_PREFIX_INFO.size ());
      return;
    }

  _LOG_DEBUG (">> info_filestate_folder: " << interest);
  m_executor.execute (bind (&StateServer::info_filestate_folder_Execute, this, interest));
}


void
StateServer::info_filestate_folder_Execute (const Ccnx::Name &interest)
{
  // <PREFIX_INFO>/"filestate"/"folder"/<one-component-relative-folder-name>/<nonce>/<offset>
  try
    {
      int offset = interest.getCompFromBackAsInt (0);

      // /// @todo !!! add security checking

      string folder;
      if (interest.size () - m_PREFIX_INFO.size () == 5)
        folder = interest.getCompFromBackAsString (2);
      else // == 4
        folder = "";

      FileStatePtr fileState = m_actionLog->GetFileState ();
      fileState->LookupFilesInFolderRecursively (debugFileState, folder, offset*100, 100);

      // m_ccnx->publishData (interest, "FAIL: Not implemented", 1);
    }
  catch (Ccnx::NameException &ne)
    {
      // ignore any unexpected interests and errors
      _LOG_ERROR (*boost::get_error_info<Ccnx::error_info_str>(ne));
    }
}


void
StateServer::cmd_restore_file (const Ccnx::Name &interest)
{
  if (interest.size () - m_PREFIX_CMD.size () != 4 &&
      interest.size () - m_PREFIX_CMD.size () != 5)
    {
      _LOG_DEBUG ("Invalid interest: " << interest);
      return;
    }

  _LOG_DEBUG (">> cmd_restore_file: " << interest);
  m_executor.execute (bind (&StateServer::cmd_restore_file_Execute, this, interest));
}

void
StateServer::cmd_restore_file_Execute (const Ccnx::Name &interest)
{
  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>

  /// @todo !!! add security checking

  try
    {
      FileItemPtr file;

      if (interest.size () - m_PREFIX_CMD.size () == 5)
        {
          Hash hash (head(interest.getCompFromBack (0)), interest.getCompFromBack (0).size());
          int64_t version = interest.getCompFromBackAsInt (1);
          string  filename = interest.getCompFromBackAsString (2); // should be safe even with full relative path

          file = m_actionLog->LookupAction (filename, version, hash);
          if (!file)
            {
              _LOG_ERROR ("Requested file is not found: [" << filename << "] version [" << version << "] hash [" << hash.shortHash () << "]");
            }
        }
      else
        {
          int64_t version = interest.getCompFromBackAsInt (0);
          string  filename = interest.getCompFromBackAsString (1); // should be safe even with full relative path

          file = m_actionLog->LookupAction (filename, version, Hash (0,0));
          if (!file)
            {
              _LOG_ERROR ("Requested file is not found: [" << filename << "] version [" << version << "]");
            }
        }

      if (!file)
        {
          m_ccnx->publishData (interest, "FAIL: Requested file is not found", 1);
          return;
        }

      Hash hash = Hash (file->file_hash ().c_str (), file->file_hash ().size ());

      ///////////////////
      // now the magic //
      ///////////////////

      boost::filesystem::path filePath = m_rootDir / file->filename ();
      Name deviceName (file->device_name ().c_str (), file->device_name ().size ());

      try
        {
          if (filesystem::exists (filePath) &&
              filesystem::last_write_time (filePath) == file->mtime () &&
              filesystem::status (filePath).permissions () == static_cast<filesystem::perms> (file->mode ()) &&
              *Hash::FromFileContent (filePath) == hash)
            {
              m_ccnx->publishData (interest, "OK: File already exists", 1);
              _LOG_DEBUG ("Asking to assemble a file, but file already exists on a filesystem");
              return;
            }
        }
      catch (filesystem::filesystem_error &error)
        {
          m_ccnx->publishData (interest, "FAIL: File operation failed", 1);
          _LOG_ERROR ("File operations failed on [" << filePath << "] (ignoring)");
        }

      _LOG_TRACE ("Restoring file [" << filePath << "]");
      if (m_objectManager.objectsToLocalFile (deviceName, hash, filePath))
        {
          last_write_time (filePath, file->mtime ());
          permissions (filePath, static_cast<filesystem::perms> (file->mode ()));
          m_ccnx->publishData (interest, "OK", 1);
        }
      else
        {
          m_ccnx->publishData (interest, "FAIL: Unknown error while restoring file", 1);
        }
    }
  catch (Ccnx::NameException &ne)
    {
      // ignore any unexpected interests and errors
      _LOG_ERROR(*boost::get_error_info<Ccnx::error_info_str>(ne));
    }
}
