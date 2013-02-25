#include "sparkle-auto-update.h"
#import <Sparkle/Sparkle.h>

class SparkleAutoUpdate::Private
{
  public:
    SUUpdater *updater;
};

SparkleAutoUpdate::SparkleAutoUpdate(const QString &updateUrl)
{
  d = new Private;
  d->updater = [SUUpdater sharedUpdater];
  [d->updater setAutomaticallyChecksForUpdates:YES];
  NSURL *url = [NSURL URLWithString: [NSString stringWithUTF8String: updateUrl.toUtf8().data()]];
  [d->updater setFeedURL: url];
}

SparkleAutoUpdate::~SparkleAutoUpdate()
{
  delete d;
  // presummably SUUpdater handles garbage collection
}

void SparkleAutoUpdate::checkForUpdates()
{
  [d->updater checkForUpdatesInBackground];
}
