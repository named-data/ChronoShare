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
  , m_scheduler (new Scheduler())
  , m_userName (userName)
  , m_sharedFolderName (sharedFolderName)
  , m_appName (appName)
{
  // may be later /localhost should be replaced with /%C1.M.S.localhost

  // <PREFIX_INFO> = /localhost/<user's-device-name>/"chronoshare"/"info"
  m_PREFIX_INFO = Name ("/localhost")(m_userName)("chronoshare")("info");

  // <PREFIX_CMD> = /localhost/<user's-device-name>/"chronoshare"/"cmd"
  m_PREFIX_CMD = Name ("/localhost")(m_userName)("chronoshare")("cmd");

  m_scheduler->start ();

  registerPrefixes ();
}

StateServer::~StateServer()
{
  m_scheduler->shutdown ();

  deregisterPrefixes ();
}

void
StateServer::registerPrefixes ()
{
  // currently supporting limited number of command.
  // will be extended to support all planned commands later

  // <PREFIX_INFO>/"actions"/"all"/<nonce>/<segment>  get list of all actions
  m_ccnx->setInterestFilter (Name (m_PREFIX_INFO)("actions")("all"), bind(&StateServer::info_actions_all, this, _1));

  // // <PREFIX_INFO>/"filestate"/"all"/<nonce>/<segment>
  // m_ccnx->setInterestFilter (Name (m_PREFIX_INFO)("filestate")("all"), bind(&StateServer::info_filestate_all, this, _1));

  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
  m_ccnx->setInterestFilter (Name (m_PREFIX_CMD)("restore")("file"), bind(&StateServer::cmd_restore_file, this, _1));
}

void
StateServer::deregisterPrefixes ()
{
  m_ccnx->clearInterestFilter (Name (m_PREFIX_INFO)("actions")("all"));
  m_ccnx->clearInterestFilter (Name (m_PREFIX_INFO)("filestate")("all"));
  m_ccnx->clearInterestFilter (Name (m_PREFIX_CMD) ("restore")("file"));
}

void
StateServer::info_actions_all (const Name &interest)
{
  _LOG_DEBUG (">> info_actions_all: " << interest);
  m_scheduler->scheduleOneTimeTask (m_scheduler, 0, bind (&StateServer::info_actions_all_Execute, this, interest), boost::lexical_cast<string>(interest));
}

void
StateServer::info_actions_all_Execute (const Name &interest)
{
  // <PREFIX_INFO>/"actions"/"all"/<nonce>/<segment>  get list of all actions

  try
    {
      int segment = interest.getCompFromBackAsInt (0);
      if (segment != 0)
        {
          // ignore anything except segment 0, other stuff should be cached.. if not, then good luck
          return;
        }

      /// @todo !!! add security checking
      m_ccnx->publishData (interest, "FAIL: Not implemented", 1);
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
  _LOG_DEBUG (">> cmd_restore_file: " << interest);
  m_scheduler->scheduleOneTimeTask (m_scheduler, 0, bind (&StateServer::cmd_restore_file_Execute, this, interest), boost::lexical_cast<string>(interest));
}

void
StateServer::cmd_restore_file_Execute (const Ccnx::Name &interest)
{
  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>

  /// @todo !!! add security checking

  try
    {
      Hash hash (head(interest.getCompFromBack (0)), interest.getCompFromBack (0).size());
      int64_t version = interest.getCompFromBackAsInt (1);
      string  filename = interest.getCompFromBackAsString (2); // should be safe even with full relative path

      FileItemPtr file = m_actionLog->LookupAction (filename, version, hash);
      if (!file)
        {
          m_ccnx->publishData (interest, "FAIL: Requested file is not found", 1);
          _LOG_ERROR ("Requested file is not found: [" << filename << "] version [" << version << "] hash [" << hash.shortHash () << "]");
          return;
        }

      hash = Hash (file->file_hash ().c_str (), file->file_hash ().size ());

      ///////////////////
      // now the magic //
      ///////////////////

      boost::filesystem::path filePath = m_rootDir / filename;
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
