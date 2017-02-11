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

#include "osx-auto-update-sparkle.hpp"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>

namespace ndn {
namespace chronoshare {

class OsxAutoUpdateSparkle::Impl
{
public:
  SUUpdater* m_updater;
};


OsxAutoUpdateSparkle::OsxAutoUpdateSparkle(const std::string& updateUrl)
  : m_impl(make_unique<Impl>())
{
  m_impl->m_updater = [[SUUpdater sharedUpdater] retain];
  NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:updateUrl.data()]];
  [m_impl->m_updater setFeedURL:url];
  [m_impl->m_updater setAutomaticallyChecksForUpdates:YES];
  [m_impl->m_updater setUpdateCheckInterval:86400];
}

OsxAutoUpdateSparkle::~OsxAutoUpdateSparkle()
{
  [m_impl->m_updater release];
  // presummably SUUpdater handles garbage collection
}

void
OsxAutoUpdateSparkle::checkForUpdates()
{
  //[m_impl->m_updater checkForUpdatesInBackground];
  [m_impl->m_updater checkForUpdates:nil];
}

} // namespace chronoshare
} // namespace ndn
