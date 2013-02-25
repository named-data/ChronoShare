#ifndef SPARKLE_AUTO_UPDATE_H
#define SPARKLE_AUTO_UPDATE_H
#include "auto-update.h"
#include <QString>
class SparkleAutoUpdate : public AutoUpdate
{
public:
  SparkleAutoUpdate (const QString &url);
  ~SparkleAutoUpdate ();
  virtual void checkForUpdates();
private:
  class Private;
  Private *d;
}
#endif
