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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "sparkle-auto-update.h"
#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>
#include <stdio.h>

class SparkleAutoUpdate::Private
{
  public:
    SUUpdater *updater;
};

SparkleAutoUpdate::SparkleAutoUpdate(const QString &updateUrl)
{
  d = new Private;
  d->updater = [[SUUpdater sharedUpdater] retain];
  NSURL *url = [NSURL URLWithString: [NSString stringWithUTF8String: updateUrl.toUtf8().data()]];
  [d->updater setFeedURL: url];
  [d->updater setAutomaticallyChecksForUpdates: YES];
  [d->updater setUpdateCheckInterval: 86400];
}

SparkleAutoUpdate::~SparkleAutoUpdate()
{
  [d->updater release];
  delete d;
  // presummably SUUpdater handles garbage collection
}

void SparkleAutoUpdate::checkForUpdates()
{
  //[d->updater checkForUpdatesInBackground];
  [d->updater checkForUpdates : nil];
  printf("++++++++ checking update ++++++\n");
}
