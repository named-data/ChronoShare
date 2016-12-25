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

#include "sparkle-auto-update.hpp"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>

#include "logging.hpp"

INIT_LOGGER("SparkeAutoUpdate");

class SparkleAutoUpdate::Private
{
public:
  SUUpdater* updater;
};

SparkleAutoUpdate::SparkleAutoUpdate(const QString& updateUrl)
{
  d = new Private;
  d->updater = [[SUUpdater sharedUpdater] retain];
  NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:updateUrl.toUtf8().data()]];
  [d->updater setFeedURL:url];
  [d->updater setAutomaticallyChecksForUpdates:YES];
  [d->updater setUpdateCheckInterval:86400];
}

SparkleAutoUpdate::~SparkleAutoUpdate()
{
  [d->updater release];
  delete d;
  // presummably SUUpdater handles garbage collection
}

void
SparkleAutoUpdate::checkForUpdates()
{
  //[d->updater checkForUpdatesInBackground];
  [d->updater checkForUpdates:nil];
  _LOG_DEBUG("++++++++ checking update +++++");
}
