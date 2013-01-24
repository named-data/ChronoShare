// /* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
// /*
//  * Copyright (c) 2012-2013 University of California, Los Angeles
//  *
//  * This program is free software; you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License version 2 as
//  * published by the Free Software Foundation;
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  * GNU General Public License for more details.
//  *
//  * You should have received a copy of the GNU General Public License
//  * along with this program; if not, write to the Free Software
//  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  *
//  * Author: Jared Lindblom <lindblom@cs.ucla.edu>
//  */

// #include "simpleeventcatcher.h"

// SimpleEventCatcher::SimpleEventCatcher(FileSystemWatcher* watcher, QObject *parent) :
//   QObject(parent)
// {
//   // register for directory event signal (callback function)
//   QObject::connect(watcher, SIGNAL(dirEventSignal(std::vector<sEventInfo>)), this, SLOT(handleDirEvent(std::vector<sEventInfo>)));
// }

// void SimpleEventCatcher::handleDirEvent(std::vector<sEventInfo> dirChanges)
// {
//   qDebug() << endl << "[SIGNAL] From SimpleEventCatcher Slot:";

//   if(!dirChanges.empty())
//     {
//       for(size_t i = 0; i < dirChanges.size(); i++)
//         {
//           QString tempString;

//           eEvent event = dirChanges[i].event;
//           QString absFilePath = QString::fromStdString(dirChanges[i].absFilePath);

//           switch(event)
//             {
//             case ADDED:
//               tempString.append("ADDED: ");
//               break;
//             case MODIFIED:
//               tempString.append("MODIFIED: ");
//               break;
//             case DELETED:
//               tempString.append("DELETED: ");
//               break;
//             }

//           tempString.append (absFilePath);

//           qDebug() << "\t" << tempString;
//         }
//     }
//   else
//     {
//       qDebug() << "\t[EMPTY]";
//     }
// }

// #if WAF
// #include "simpleeventcatcher.moc"
// #include "simpleeventcatcher.cpp.moc"
// #endif
