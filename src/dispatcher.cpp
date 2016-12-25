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

#include "dispatcher.hpp"
#include "fetch-task-db.hpp"
#include "core/logging.hpp"

#include <ndn-cxx/util/digest.hpp>
#include <ndn-cxx/util/string-helper.hpp>

#include <boost/lexical_cast.hpp>

namespace ndn {
namespace chronoshare {

_LOG_INIT(Dispatcher);

namespace fs = boost::filesystem;

static const name::Component CHRONOSHARE_APP = name::Component("chronoshare");
static const Name BROADCAST_DOMAIN = "/ndn/multicast";

static const time::seconds DEFAULT_SYNC_INTEREST_INTERVAL = time::seconds(10);

Dispatcher::Dispatcher(const std::string& localUserName, const std::string& sharedFolder,
                       const fs::path& rootDir, Face& face, bool enablePrefixDiscovery)
  : m_face(face)
  , m_rootDir(rootDir)
  , m_ioService(face.getIoService())
  , m_objectManager(face, m_keyChain, rootDir, CHRONOSHARE_APP.toUri())
  , m_localUserName(localUserName)
  , m_sharedFolder(sharedFolder)
  , m_enablePrefixDiscovery(enablePrefixDiscovery)
{
  m_syncLog = make_shared<SyncLog>(m_rootDir, localUserName);
  m_actionLog =
    make_shared<ActionLog>(m_face, m_rootDir, m_syncLog, sharedFolder, CHRONOSHARE_APP,
                           // bind(&Dispatcher::Did_ActionLog_ActionApply_AddOrModify, this, _1, _2, _3, _4, _5, _6, _7),
                           ActionLog::OnFileAddedOrChangedCallback(), // don't really need this callback
                           bind(&Dispatcher::Did_ActionLog_ActionApply_Delete, this, _1));
  m_fileState = m_actionLog->GetFileState();

  Name syncPrefix = Name(BROADCAST_DOMAIN);
  syncPrefix.append(CHRONOSHARE_APP);
  syncPrefix.append(sharedFolder);

  m_server = make_unique<ContentServer>(m_face, m_actionLog, rootDir, m_localUserName,
                                        m_sharedFolder, CHRONOSHARE_APP, m_keyChain);
  m_server->registerPrefix(Name("/"));
  m_server->registerPrefix(Name(BROADCAST_DOMAIN));

  //m_stateServer = make_unique<StateServer>(m_face, m_actionLog, rootDir, m_localUserName, m_sharedFolder,
  //                                         CHRONOSHARE_APP, m_objectManager, m_keyChair);

  m_core = make_unique<SyncCore>(m_face, m_syncLog, localUserName, Name("/"), syncPrefix,
                                 bind(&Dispatcher::Did_SyncLog_StateChange, this, _1),
                                 DEFAULT_SYNC_INTEREST_INTERVAL);

  FetchTaskDbPtr actionTaskDb = make_shared<FetchTaskDb>(m_rootDir, "action");
  m_actionFetcher =
    make_shared<FetchManager>(m_face, bind(&SyncLog::LookupLocator, &*m_syncLog, _1),
                              Name(BROADCAST_DOMAIN), // no appname suffix now
                              3,
                              bind(&Dispatcher::Did_FetchManager_ActionFetch, this, _1, _2, _3, _4),
                              FetchManager::FinishCallback(), actionTaskDb);

  FetchTaskDbPtr fileTaskDb = make_shared<FetchTaskDb>(m_rootDir, "file");
  m_fileFetcher =
    make_shared<FetchManager>(m_face, bind(&SyncLog::LookupLocator, &*m_syncLog, _1),
                              Name(BROADCAST_DOMAIN), // no appname suffix now
                              3, bind(&Dispatcher::Did_FetchManager_FileSegmentFetch, this, _1, _2,
                                      _3, _4),
                              bind(&Dispatcher::Did_FetchManager_FileFetchComplete, this, _1, _2),
                              fileTaskDb);

  if (m_enablePrefixDiscovery) {
    _LOG_DEBUG("registering prefix discovery in Dispatcher");
    std::string tag = "dispatcher" + m_localUserName.toUri();
    // Ccnx::CcnxDiscovery::registerCallback(TaggedFunction(bind(&Dispatcher::Did_LocalPrefix_Updated,
    // this, _1), tag));
    // TODO registerCallback...?
    //
    // this registerCallback is used when the local prefix changes.
    // the ndn-cxx library does not have this functionality
    // thus, the application will need to implement this.
    // send a data packet and get the local prefix. If they are different, call the callback
    // function, else do nothing.
  }
}

Dispatcher::~Dispatcher()
{
  _LOG_DEBUG("Enter destructor of dispatcher");

  if (m_enablePrefixDiscovery) {
    _LOG_DEBUG("deregistering prefix discovery in Dispatcher");
    std::string tag = "dispatcher" + m_localUserName.toUri();
    // TODO
    //    Ccnx::CcnxDiscovery::deregisterCallback(TaggedFunction(bind(&Dispatcher::Did_LocalPrefix_Updated,
    //    this, _1), tag));
  }
}

void
Dispatcher::Did_LocalPrefix_Updated(const Name& forwardingHint)
{
  Name effectiveForwardingHint;
  if (m_localUserName.size() >= forwardingHint.size() &&
      m_localUserName.getSubName(0, forwardingHint.size()) == forwardingHint) {
    effectiveForwardingHint = Name("/"); // "directly" accesible
  }
  else {
    effectiveForwardingHint = forwardingHint;
  }

  Name oldLocalPrefix = m_syncLog->LookupLocalLocator();

  if (oldLocalPrefix == effectiveForwardingHint) {
    _LOG_DEBUG(
      "Got notification about prefix change from " << oldLocalPrefix << " to: " << forwardingHint
                                                   << ", but effective prefix didn't change");
    return;
  }

  if (effectiveForwardingHint == Name("/") || effectiveForwardingHint == Name(BROADCAST_DOMAIN)) {
    _LOG_DEBUG("Basic effective prefix [" << effectiveForwardingHint
                                          << "]. Updating local prefix, but don't reregister");
    m_syncLog->UpdateLocalLocator(effectiveForwardingHint);
    return;
  }

  _LOG_DEBUG("LocalPrefix changed from: " << oldLocalPrefix << " to: " << effectiveForwardingHint);

  m_server->registerPrefix(effectiveForwardingHint);
  m_syncLog->UpdateLocalLocator(effectiveForwardingHint);

  if (oldLocalPrefix == Name("/") || oldLocalPrefix == Name(BROADCAST_DOMAIN)) {
    _LOG_DEBUG("Don't deregister basic prefix: " << oldLocalPrefix);
    return;
  }
  m_server->deregisterPrefix(oldLocalPrefix);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void
Dispatcher::Did_LocalFile_AddOrModify(const fs::path& relativeFilePath)
{
  m_ioService.post(bind(&Dispatcher::Did_LocalFile_AddOrModify_Execute, this, relativeFilePath));
}

void
Dispatcher::Did_LocalFile_AddOrModify_Execute(fs::path relativeFilePath)
{
  _LOG_DEBUG(m_localUserName << " calls LocalFile_AddOrModify_Execute");
  fs::path absolutePath = m_rootDir / relativeFilePath;
  _LOG_DEBUG("relativeFilePath : " << relativeFilePath);
  _LOG_DEBUG("absolutePath : " << absolutePath);
  if (!fs::exists(absolutePath)) {
    // BOOST_THROW_EXCEPTION(Error::Dispatcher() << error_info_str("Update non exist file: " +
    // absolutePath.string() ));
    _LOG_DEBUG("Update non exist file: " << absolutePath.string());
    return;
  }

  FileItemPtr currentFile = m_fileState->LookupFile(relativeFilePath.generic_string());

  if (currentFile) {
    fs::ifstream input(absolutePath);
    if (*util::Sha256(input).computeDigest() ==
        Buffer(currentFile->file_hash().c_str(), currentFile->file_hash().size())
        // The following two are commented out to prevent front end from reporting intermediate files
        // should enable it if there is other way to prevent this
        // && last_write_time(absolutePath) == currentFile->mtime()
        // && status(absolutePath).permissions() ==
        // static_cast<fs::perms>(currentFile->mode())
        ) {
      _LOG_ERROR("Got notification about the same file [" << relativeFilePath << "]");
      return;
    }
  }

  if (currentFile && !currentFile->is_complete()) {
    _LOG_ERROR("Got notification about incomplete file [" << relativeFilePath << "]");
    return;
  }

  int seg_num;
  ConstBufferPtr hash;
  _LOG_DEBUG("absolutePath: " << absolutePath << " m_localUserName: " << m_localUserName);
  tie(hash, seg_num) = m_objectManager.localFileToObjects(absolutePath, m_localUserName);

  try {
    m_actionLog->AddLocalActionUpdate(relativeFilePath.generic_string(), *hash,
                                      last_write_time(absolutePath),
#if BOOST_VERSION >= 104900
                                      status(absolutePath).permissions(),
#else
                                      0,
#endif
                                      seg_num);

    // notify SyncCore to propagate the change
    m_core->localStateChangedDelayed();
  }
  catch (fs::filesystem_error& error) {
    _LOG_ERROR("File operations failed on [" << relativeFilePath << "](ignoring)");
  }

  _LOG_DEBUG("LocalFile_AddOrModify_Execute Finished!");
}

void
Dispatcher::Did_LocalFile_Delete(const fs::path& relativeFilePath)
{
  m_ioService.post(bind(&Dispatcher::Did_LocalFile_Delete_Execute, this, relativeFilePath));
}

void
Dispatcher::Did_LocalFile_Delete_Execute(fs::path relativeFilePath)
{
  fs::path absolutePath = m_rootDir / relativeFilePath;
  if (fs::exists(absolutePath)) {
    _LOG_ERROR("DELETE command, but file still exists: " << absolutePath.string());
    return;
  }

  m_actionLog->AddLocalActionDelete(relativeFilePath.generic_string());
  // notify SyncCore to propagate the change
  m_core->localStateChangedDelayed();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void
Dispatcher::Did_SyncLog_StateChange(SyncStateMsgPtr stateMsg)
{
  m_ioService.post(bind(&Dispatcher::Did_SyncLog_StateChange_Execute, this, stateMsg));
}

void
Dispatcher::Did_SyncLog_StateChange_Execute(SyncStateMsgPtr stateMsg)
{
  int size = stateMsg->state_size();
  int index = 0;
  // iterate and fetch the actions
  for (; index < size; index++) {
    SyncState state = stateMsg->state(index);
    if (state.has_old_seq() && state.has_seq()) {
      uint64_t oldSeq = state.old_seq();
      uint64_t newSeq = state.seq();
      Name userName(Block((const unsigned char*)state.name().c_str(), state.name().size()));

      // fetch actions with oldSeq + 1 to newSeq(inclusive)
      Name actionNameBase = Name("/");
      actionNameBase.append(userName).append(CHRONOSHARE_APP).append("action").append(m_sharedFolder);
      m_actionFetcher->Enqueue(userName, actionNameBase, std::max<uint64_t>(oldSeq + 1, 1), newSeq,
                               FetchManager::PRIORITY_HIGH);
    }
  }
}

void
Dispatcher::Did_FetchManager_ActionFetch(const Name& deviceName, const Name& actionBaseName,
                                         uint32_t seqno, shared_ptr<Data> actionData)
{
  /// @todo Errors and exception checking
  _LOG_DEBUG("Received action deviceName: " << deviceName << ", actionBaseName: " << actionBaseName
                                            << ", seqno: "
                                            << seqno);

  ActionItemPtr action = m_actionLog->AddRemoteAction(deviceName, seqno, actionData);
  if (!action) {
    _LOG_ERROR("AddRemoteAction did not insert action, ignoring");
    return;
  }
  // trigger may invoke Did_ActionLog_ActionApply_Delete or Did_ActionLog_ActionApply_AddOrModify
  // callbacks

  if (action->action() == ActionItem::UPDATE) {
    ConstBufferPtr hash =
      make_shared<Buffer>(action->file_hash().c_str(), action->file_hash().size());

    Name fileNameBase = Name("/");
    fileNameBase.append(deviceName).append(CHRONOSHARE_APP).append("file");
    //      fileNameBase.append(name::Component(hash));
    fileNameBase.appendImplicitSha256Digest(hash);

    std::string hashStr = toHex(*hash);
    if (ObjectDb::doesExist(m_rootDir / ".chronoshare", deviceName, hashStr)) {
      _LOG_DEBUG("File already exists in the database. No need to refetch, just directly applying "
                 "the action");
      Did_FetchManager_FileFetchComplete(deviceName, fileNameBase);
    }
    else {
      if (m_objectDbMap.find(*hash) == m_objectDbMap.end()) {
        _LOG_DEBUG("create ObjectDb for " << toHex(*hash));
        m_objectDbMap[*hash] = make_shared<ObjectDb>(m_rootDir / ".chronoshare", hashStr);
      }

      m_fileFetcher->Enqueue(deviceName, fileNameBase, 0, action->seg_num() - 1,
                             FetchManager::PRIORITY_NORMAL);
    }
  }
}

void
Dispatcher::Did_ActionLog_ActionApply_Delete(const std::string& filename)
{
  _LOG_DEBUG("Action to delete " << filename);

  fs::path absolutePath = m_rootDir / filename;
  try {
    if (fs::exists(absolutePath)) {
      // need some protection from local detection of removal
      remove(absolutePath);

      // hack to remove empty parent dirs
      fs::path parentPath = absolutePath.parent_path();
      while (parentPath > m_rootDir) {
        if (fs::is_empty(parentPath)) {
          fs::remove(parentPath);
          parentPath = parentPath.parent_path();
        }
        else {
          break;
        }
      }
    }
    // don't exist
  }
  catch (fs::filesystem_error& error) {
    _LOG_ERROR("File operations failed when removing [" << absolutePath << "](ignoring)");
  }
}

void
Dispatcher::Did_FetchManager_FileSegmentFetch(const Name& deviceName,
                                              const Name& fileSegmentBaseName,
                                              uint32_t segment,
                                              shared_ptr<Data>
                                                fileSegmentData)
{
  m_ioService.post(bind(&Dispatcher::Did_FetchManager_FileSegmentFetch_Execute, this, deviceName,
                        fileSegmentBaseName, segment, fileSegmentData));
}

void
Dispatcher::Did_FetchManager_FileSegmentFetch_Execute(Name deviceName,
                                                      Name fileSegmentBaseName,
                                                      uint32_t segment,
                                                      shared_ptr<Data> fileSegmentData)
{
  // fileSegmentBaseName:  /<device_name>/<appname>/file/<hash>

  Buffer hash(fileSegmentBaseName.get(-1).value(), fileSegmentBaseName.get(-1).value_size());

  _LOG_DEBUG("Received segment deviceName: " << deviceName << ", segmentBaseName: " << fileSegmentBaseName
                                             << ", segment: "
                                             << segment);

  // _LOG_DEBUG("Looking up objectdb for " << hash);

  std::map<Buffer, shared_ptr<ObjectDb>>::iterator db = m_objectDbMap.find(hash);
  if (db != m_objectDbMap.end()) {
    db->second->saveContentObject(deviceName, segment, *fileSegmentData);
  }
  else {
    _LOG_ERROR("no db available for this content object: " << fileSegmentBaseName << ", size: "
                                                           << fileSegmentData->getContent().size());
  }

  // ObjectDb objectDb(m_rootDir / ".chronoshare", lexical_cast<string>(hash));
  // objectDb.saveContentObject(deviceName, segment, *fileSegmentData);
}

void
Dispatcher::Did_FetchManager_FileFetchComplete(const Name& deviceName, const Name& fileBaseName)
{
  m_ioService.post(
    bind(&Dispatcher::Did_FetchManager_FileFetchComplete_Execute, this, deviceName, fileBaseName));
}

void
Dispatcher::Did_FetchManager_FileFetchComplete_Execute(Name deviceName, Name fileBaseName)
{
  // fileBaseName:  /<device_name>/<appname>/file/<hash>

  _LOG_DEBUG("Finished fetching " << deviceName << ", fileBaseName: " << fileBaseName);

  Buffer hash(fileBaseName.get(-1).value(), fileBaseName.get(-1).value_size());

  _LOG_DEBUG("Extracted hash: " << toHex(hash));

  if (m_objectDbMap.find(hash) != m_objectDbMap.end()) {
    // remove the db handle
    m_objectDbMap.erase(hash); // to commit write
  }
  else {
    _LOG_ERROR("no db available for this file: " << toHex(hash));
  }

  FileItemsPtr filesToAssemble = m_fileState->LookupFilesForHash(hash);

  for (FileItems::iterator file = filesToAssemble->begin(); file != filesToAssemble->end(); file++) {
    fs::path filePath = m_rootDir / file->filename();

    try {
      if (fs::exists(filePath) && fs::last_write_time(filePath) == file->mtime() &&
          fs::status(filePath).permissions() == static_cast<fs::perms>(file->mode())) {
          fs::ifstream input(filePath, std::ios::in | std::ios::binary);
          if (*util::Sha256(input).computeDigest() == hash) {
            _LOG_DEBUG("Asking to assemble a file, but file already exists on a filesystem");
            continue;
          }
      }
    }
    catch (fs::filesystem_error& error) {
      _LOG_ERROR("File operations failed on [" << filePath << "](ignoring)");
    }

    if (ObjectDb::doesExist(m_rootDir / ".chronoshare", deviceName, toHex(hash))) {
      bool ok = m_objectManager.objectsToLocalFile(deviceName, hash, filePath);
      if (ok) {
        last_write_time(filePath, file->mtime());
#if BOOST_VERSION >= 104900
        permissions(filePath, static_cast<fs::perms>(file->mode()));
#endif

        m_fileState->SetFileComplete(file->filename());
      }
      else {
        _LOG_ERROR("Notified about complete fetch, but file cannot be restored from the database: ["
                   << filePath
                   << "]");
      }
    }
    else {
      _LOG_ERROR(filePath << " supposed to have all segments, but not");
      // should abort for debugging
    }
  }
}

} // namespace chronoshare
} // namespace ndn
