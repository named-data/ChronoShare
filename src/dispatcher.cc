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

#include "dispatcher.h"
#include "logging.h"

#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

INIT_LOGGER ("Dispatcher");

static const string BROADCAST_DOMAIN = "/ndn/broadcast/chronoshare";

Dispatcher::Dispatcher(const filesystem::path &path, const std::string &localUserName,
                       const Ccnx::Name &localPrefix, const std::string &sharedFolder,
                       const filesystem::path &rootDir, Ccnx::CcnxWrapperPtr ccnx,
                       SchedulerPtr scheduler, int poolSize)
           : m_ccnx(ccnx)
           , m_core(NULL)
           , m_rootDir(rootDir)
           , m_executor(poolSize)
           , m_objectManager(ccnx, rootDir)
           , m_localUserName(localUserName)
           , m_sharedFolder(sharedFolder)
           , m_server(NULL)
{
  m_syncLog = make_shared<SyncLog>(path, localUserName);
  m_actionLog = make_shared<ActionLog>(m_ccnx, path, m_syncLog, sharedFolder,
                                       // bind (&Dispatcher::Did_ActionLog_ActionApply_AddOrModify, this, _1, _2, _3, _4, _5, _6, _7),
                                       ActionLog::OnFileAddedOrChangedCallback (), // don't really need this callback
                                       bind (&Dispatcher::Did_ActionLog_ActionApply_Delete, this, _1));
  Name syncPrefix = Name(BROADCAST_DOMAIN)(sharedFolder);

  m_server = new ContentServer(m_ccnx, m_actionLog, rootDir);
  m_server->registerPrefix(localPrefix);
  m_server->registerPrefix(syncPrefix);

  m_core = new SyncCore (m_syncLog, localUserName, localPrefix, syncPrefix,
                         bind(&Dispatcher::Did_SyncLog_StateChange, this, _1), ccnx, scheduler);

  m_actionFetcher = make_shared<FetchManager> (m_ccnx, bind (&SyncLog::LookupLocator, &*m_syncLog, _1), 3);
  m_fileFetcher   = make_shared<FetchManager> (m_ccnx, bind (&SyncLog::LookupLocator, &*m_syncLog, _1), 3);
}

Dispatcher::~Dispatcher()
{
  if (m_core != NULL)
  {
    delete m_core;
    m_core = NULL;
  }

  if (m_server != NULL)
  {
    delete m_server;
    m_server = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void
Dispatcher::Did_LocalFile_AddOrModify (const filesystem::path &relativeFilePath)
{
  m_executor.execute (bind (&Dispatcher::Did_LocalFile_AddOrModify_Execute, this, relativeFilePath));
}

void
Dispatcher::Did_LocalFile_AddOrModify_Execute (filesystem::path relativeFilePath)
{
  filesystem::path absolutePath = m_rootDir / relativeFilePath;
  if (!filesystem::exists(absolutePath))
    {
      BOOST_THROW_EXCEPTION (Error::Dispatcher() << error_info_str("Update non exist file: " + absolutePath.string() ));
    }

  FileItemPtr currentFile = m_actionLog->LookupFile (relativeFilePath.generic_string ());
  if (currentFile &&
      *Hash::FromFileContent (absolutePath) == Hash (currentFile->file_hash ().c_str (), currentFile->file_hash ().size ()) &&
      last_write_time (absolutePath) == currentFile->mtime () &&
      status (absolutePath).permissions () == static_cast<filesystem::perms> (currentFile->mode ()))
    {
      _LOG_ERROR ("Got notification about the same file [" << relativeFilePath << "]");
      return;
    }

  int seg_num;
  HashPtr hash;
  tie (hash, seg_num) = m_objectManager.localFileToObjects (absolutePath, m_localUserName);

  m_actionLog->AddLocalActionUpdate (relativeFilePath.generic_string(),
                                     *hash,
                                     last_write_time (absolutePath), status (absolutePath).permissions (), seg_num);

  // notify SyncCore to propagate the change
  m_core->localStateChanged();
}

void
Dispatcher::Did_LocalFile_Delete (const filesystem::path &relativeFilePath)
{
  m_executor.execute (bind (&Dispatcher::Did_LocalFile_Delete_Execute, this, relativeFilePath));
}

void
Dispatcher::Did_LocalFile_Delete_Execute (filesystem::path relativeFilePath)
{
  filesystem::path absolutePath = m_rootDir / relativeFilePath;
  if (!filesystem::exists(absolutePath))
    {
      BOOST_THROW_EXCEPTION (Error::Dispatcher() << error_info_str("Delete notification but file exists: " + absolutePath.string() ));
    }

  FileItemPtr currentFile = m_actionLog->LookupFile (relativeFilePath.generic_string ());
  if (!currentFile)
    {
      _LOG_ERROR ("File already deleted [" << relativeFilePath << "]");
      return;
    }

  m_actionLog->AddLocalActionDelete (relativeFilePath.generic_string());
  // notify SyncCore to propagate the change
  m_core->localStateChanged();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Callbacks:
 *
 * - from SyncLog: when state changes -> to fetch missing actions
 *
 * - from FetchManager/Actions: when action is fetched -> to request a file, specified by the action
 *                                                     -> to add action to the action log
 *
 * - from ActionLog/Delete:      when action applied (file state changed, file deleted)           -> to delete local file
 *
 * - from ActionLog/AddOrUpdate: when action applied (file state changes, file added or modified) -> do nothing?
 *
 * - from FetchManager/Files: when file segment is retrieved -> save it in ObjectDb
 *                            when file fetch is completed   -> if file belongs to FileState, then assemble it to filesystem. Don't do anything otherwise
 */

void
Dispatcher::Did_SyncLog_StateChange (const SyncStateMsgPtr &stateMsg)
{
  int size = stateMsg->state_size();
  int index = 0;
  // iterate and fetch the actions
  while (index < size)
  {
    SyncState state = stateMsg->state(index);
    if (state.has_old_seq() && state.has_seq())
    {
      uint64_t oldSeq = state.old_seq();
      uint64_t newSeq = state.seq();
      Name userName = state.name();

      // fetch actions with oldSeq + 1 to newSeq (inclusive)
      Name actionNameBase = Name(userName)("action")(m_sharedFolder);

      m_actionFetcher->Enqueue (userName, actionNameBase,
                                bind (&Dispatcher::Did_FetchManager_ActionFetch, this, _1, _2, _3, _4), FetchManager::FinishCallback (),
                                oldSeq + 1, newSeq, FetchManager::PRIORITY_HIGH);
    }
  }
}

void
Dispatcher::Did_FetchManager_ActionFetch (const Ccnx::Name &deviceName, const Ccnx::Name &actionName, uint32_t seqno, Ccnx::PcoPtr actionPco)
{
  /// @todo Errors and exception checking
  _LOG_DEBUG ("Received action deviceName: " << deviceName << ", actionName: " << actionName << ", seqno: " << seqno);

  ActionItemPtr action = m_actionLog->AddRemoteAction (deviceName, seqno, actionPco);
  // trigger may invoke Did_ActionLog_ActionApply_Delete or Did_ActionLog_ActionApply_AddOrModify callbacks

  if (action->action () == ActionItem::UPDATE)
    {
      Hash hash (action->file_hash ().c_str(), action->file_hash ().size ());

      Name fileNameBase = Name (deviceName)("file")(hash.GetHash (), hash.GetHashBytes ());

      if (m_objectDbMap.find (hash) == m_objectDbMap.end ())
        {
          m_objectDbMap [hash] = make_shared<ObjectDb> (m_rootDir, lexical_cast<string> (hash));
        }

      m_fileFetcher->Enqueue (deviceName, fileNameBase,
                              bind (&Dispatcher::Did_FetchManager_FileSegmentFetch, this, _1, _2, _3, _4),
                              bind (&Dispatcher::Did_FetchManager_FileFetchComplete, this, _1, _2),
                              0, action->seg_num (), FetchManager::PRIORITY_NORMAL);
    }
}

void
Dispatcher::Did_ActionLog_ActionApply_Delete (const std::string &filename)
{
  m_executor.execute (bind (&Dispatcher::Did_ActionLog_ActionApply_Delete_Execute, this, filename));
}

void
Dispatcher::Did_ActionLog_ActionApply_Delete_Execute (std::string filename)
{
  _LOG_DEBUG ("Action to delete " << filename);

  filesystem::path absolutePath = m_rootDir / filename;
  if (filesystem::exists(absolutePath))
    {
      // need some protection from local detection of removal
      remove (absolutePath);
    }
  // don't exist
}

void
Dispatcher::Did_FetchManager_FileSegmentFetch (const Ccnx::Name &deviceName, const Ccnx::Name &fileSegmentName, uint32_t segment, Ccnx::PcoPtr fileSegmentPco)
{
  m_executor.execute (bind (&Dispatcher::Did_FetchManager_FileSegmentFetch_Execute, this, deviceName, fileSegmentName, segment, fileSegmentPco));
}

void
Dispatcher::Did_FetchManager_FileSegmentFetch_Execute (Ccnx::Name deviceName, Ccnx::Name fileSegmentName, uint32_t segment, Ccnx::PcoPtr fileSegmentPco)
{
  const Bytes &hashBytes = fileSegmentName.getCompFromBack (1);
  Hash hash (head(hashBytes), hashBytes.size());

  _LOG_DEBUG ("Received segment deviceName: " << deviceName << ", segmentName: " << fileSegmentName << ", segment: " << segment);

  map<Hash, ObjectDbPtr>::iterator db = m_objectDbMap.find (hash);
  if (db != m_objectDbMap.end())
  {
    db->second->saveContentObject(deviceName, segment, fileSegmentPco->buf ());
  }
  else
  {
    _LOG_ERROR ("no db available for this content object: " << fileSegmentName << ", size: " << fileSegmentPco->buf ().size());
  }
}

void
Dispatcher::Did_FetchManager_FileFetchComplete (const Ccnx::Name &deviceName, const Ccnx::Name &fileBaseName)
{
  m_executor.execute (bind (&Dispatcher::Did_FetchManager_FileFetchComplete_Execute, this, deviceName, fileBaseName));
}

void
Dispatcher::Did_FetchManager_FileFetchComplete_Execute (Ccnx::Name deviceName, Ccnx::Name fileBaseName)
{
  _LOG_DEBUG ("Finished fetching " << deviceName << ", fileBaseName: " << fileBaseName);

  const Bytes &hashBytes = fileBaseName.getCompFromBack (1);
  Hash hash (head (hashBytes), hashBytes.size ());

  FileItemsPtr filesToAssemble = m_actionLog->LookupFilesForHash (hash);

  for (FileItems::iterator file = filesToAssemble->begin ();
       file != filesToAssemble->end ();
       file++)
    {
      boost::filesystem::path filePath = m_rootDir / file->filename ();
      m_objectManager.objectsToLocalFile (deviceName, hash, filePath);

      last_write_time (filePath, file->mtime ());
      permissions (filePath, static_cast<filesystem::perms> (file->mode ()));
    }

  if (m_objectDbMap.find (hash) != m_objectDbMap.end())
  {
    // remove the db handle
    m_objectDbMap.erase (hash);
  }
  else
  {
    _LOG_ERROR ("no db available for this file: " << hash);
  }
}
