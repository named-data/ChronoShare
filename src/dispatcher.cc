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
#include <boost/make_shared.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

static const string BROADCAST_DOMAIN = "/ndn/broadcast/chronoshare";
static const int MAX_FILE_SEGMENT_SIZE = 1024;

Dispatcher::Dispatcher(const filesystem::path &path, const std::string &localUserName, const Ccnx::Name &localPrefix, const std::string &sharedFolder, const filesystem::path &rootDir, Ccnx::CcnxWrapperPtr ccnx, SchedulerPtr scheduler, int poolSize)
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
  m_actionLog = make_shared<ActionLog>(m_ccnx, path, m_syncLog, localUserName, sharedFolder);
  Name syncPrefix(BROADCAST_DOMAIN + sharedFolder);

  m_server = new ContentServer(m_ccnx, m_actionLog, rootDir);
  m_server->registerPrefix(localPrefix);
  m_server->registerPrefix(syncPrefix);

  m_core = new SyncCore (m_syncLog, localUserName, localPrefix, syncPrefix,
                         bind(&Dispatcher::syncStateChanged, this, _1), ccnx, scheduler);

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

void
Dispatcher::fileChangedCallback(const filesystem::path &relativeFilePath, ActionType type)
{
  Executor::Job job = bind(&Dispatcher::fileChanged, this, relativeFilePath, type);
  m_executor.execute(job);
}

void
Dispatcher::syncStateChangedCallback(const SyncStateMsgPtr &stateMsg)
{
  Executor::Job job = bind(&Dispatcher::syncStateChanged, this, stateMsg);
  m_executor.execute(job);
}

void
Dispatcher::actionReceivedCallback(const ActionItemPtr &actionItem)
{
  Executor::Job job = bind(&Dispatcher::actionReceived, this, actionItem);
  m_executor.execute(job);
}

void
Dispatcher::fileSegmentReceivedCallback(const Ccnx::Name &name, const Ccnx::Bytes &content)
{
  Executor::Job job = bind(&Dispatcher::fileSegmentReceived, this, name, content);
  m_executor.execute(job);
}

void
Dispatcher::fileReadyCallback(const Ccnx::Name &fileNamePrefix)
{
  Executor::Job job = bind(&Dispatcher::fileReady, this, fileNamePrefix);
  m_executor.execute(job);
}

void
Dispatcher::fileChanged(const filesystem::path &relativeFilePath, ActionType type)
{

  switch (type)
  {
    case UPDATE:
    {
      filesystem::path absolutePath = m_rootDir / relativeFilePath;
      if (filesystem::exists(absolutePath))
      {
        HashPtr hash = Hash::FromFileContent(absolutePath);
        if (m_actionLog->KnownFileState(relativeFilePath.string(), *hash))
        {
          // the file state is known; i.e. the detected changed file is identical to
          // the file state kept in FileState table
          // it is the case that backend puts the file fetched from remote;
          // we should not publish action for this.
        }
        else
        {
          uintmax_t fileSize = filesystem::file_size(absolutePath);
          int seg_num = fileSize / MAX_FILE_SEGMENT_SIZE + ((fileSize % MAX_FILE_SEGMENT_SIZE == 0) ? 0 : 1);
          time_t wtime = filesystem::last_write_time(absolutePath);
          filesystem::file_status stat = filesystem::status(absolutePath);
          int mode = stat.permissions();
          m_actionLog->AddActionUpdate (relativeFilePath.string(), *hash, wtime, mode, seg_num);
          // publish the file
          m_objectManager.localFileToObjects(relativeFilePath, m_localUserName);
          // notify SyncCore to propagate the change
          m_core->localStateChanged();
        }
        break;
      }
      else
      {
        BOOST_THROW_EXCEPTION (Error::Dispatcher() << error_info_str("Update non exist file: " + absolutePath.string() ));
      }
    }
    case DELETE:
    {
      m_actionLog->AddActionDelete (relativeFilePath.string());
      // notify SyncCore to propagate the change
      m_core->localStateChanged();
      break;
    }
    default:
      break;
  }
}

void
Dispatcher::syncStateChanged(const SyncStateMsgPtr &stateMsg)
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
      Name locator = state.locator();

      // fetch actions with oldSeq + 1 to newSeq (inclusive)
      Name actionNameBase(userName);
      actionNameBase.appendComp("action")
                .appendComp(m_sharedFolder);

      for (uint64_t seqNo = oldSeq + 1; seqNo <= newSeq; seqNo++)
      {
        Name actionName = actionNameBase;
        actionName.appendComp(seqNo);

        // TODO:
        // use fetcher to fetch the name with callback "actionRecieved"
      }
    }
  }
}

void
Dispatcher::actionReceived(const ActionItemPtr &actionItem)
{
  switch (actionItem->action())
  {
    case ActionItem::UPDATE:
    {
      // @TODO
      // need a function in ActionLog to apply received action, i.e. record remote action in ActionLog

      string hashBytes = actionItem->file_hash();
      Hash hash(hashBytes.c_str(), hashBytes.size());
      ostringstream oss;
      oss << hash;
      string hashString = oss.str();
      ObjectDbPtr db = make_shared<ObjectDb>(m_rootDir, hashString);
      m_objectDbMap[hashString] = db;

      // TODO:
      // user fetcher to fetch the file with callback "fileSegmentReceived" for segment callback and "fileReady" for file ready callback
      break;
    }
    case ActionItem::DELETE:
    {
      string filename = actionItem->filename();
      // TODO:
      // m_actionLog->AddRemoteActionDelete(filename);
      break;
    }
    default:
      break;
  }
}

void
Dispatcher::fileSegmentReceived(const Ccnx::Name &name, const Ccnx::Bytes &content)
{
  int size = name.size();
  uint64_t segment = name.getCompAsInt(size - 1);
  Bytes hashBytes = name.getComp(size - 2);
  Hash hash(head(hashBytes), hashBytes.size());
  ostringstream oss;
  oss << hash;
  string hashString = oss.str();
  if (m_objectDbMap.find(hashString) != m_objectDbMap.end())
  {
    ObjectDbPtr db = m_objectDbMap[hashString];
    // get the device name
    // Name deviceName = name.getPartialName();
    // db->saveContenObject(deviceName, segment, content);
  }
  else
  {
    cout << "no db available for this content object: " << name << ", size: " << content.size() << endl;
  }
}

void
Dispatcher::fileReady(const Ccnx::Name &fileNamePrefix)
{
  int size = fileNamePrefix.size();
  Bytes hashBytes = fileNamePrefix.getComp(size - 1);
  Hash hash(head(hashBytes), hashBytes.size());
  ostringstream oss;
  oss << hash;
  string hashString = oss.str();

  if (m_objectDbMap.find(hashString) != m_objectDbMap.end())
  {
    // remove the db handle
    m_objectDbMap.erase(hashString);
  }
  else
  {
    cout << "no db available for this file: " << fileNamePrefix << endl;
  }

  // query the action table to get the path on local file system
  // m_objectManager.objectsToLocalFile(deviceName, hash, relativeFilePath);

}
