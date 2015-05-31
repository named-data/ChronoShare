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

#include "core/chronoshare-common.hpp"

#include <QtCore>

#ifndef Q_MOC_RUN
#include "dispatcher.hpp"
#include "fs-watcher/fs-watcher.hpp"
#include "core/logging.hpp"

#include <boost/asio/io_service.hpp>
#endif // Q_MOC_RUN


namespace ndn {
namespace chronoshare {

class Runner : public QObject
{
  Q_OBJECT
public:
  Runner(QObject* parent = 0)
    : QObject(parent)
    , retval(0)
  {
  }

signals:
  void
  terminateApp();

public slots:
  void
  notifyAsioThread()
  {
    emit terminateApp();
  }

public:
  int retval;
};

} // namespace chronoshare
} // namespace ndn