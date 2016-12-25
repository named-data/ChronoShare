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

#include "adhoc.hpp"
#include "core/chronoshare-config.hpp"

#if (__APPLE__ && HAVE_COREWLAN)

#include "logging.hpp"
#include <sstream>

using namespace std;

INIT_LOGGER("Adhoc.OSX");

#import <CoreWLAN/CWInterface.h>
#import <CoreWLAN/CoreWLAN.h>
#import <CoreWLAN/CoreWLANConstants.h>
#import <CoreWLAN/CoreWLANTypes.h>

const NSUInteger g_channel = 11;
static NSString* g_priorNetwork = 0;

bool
Adhoc::CreateAdhoc()
{
  NSString* networkName =
    [[NSString alloc] initWithCString:"NDNdirect" encoding:NSASCIIStringEncoding];
  NSString* passphrase =
    [[NSString alloc] initWithCString:"NDNhello" encoding:NSASCIIStringEncoding];
  NSString* securityMode = [[NSString alloc] initWithCString:"Open" encoding:NSASCIIStringEncoding];

  NSArray* airportInterfaces = [[CWInterface interfaceNames] allObjects];

  //Choose the desired interface . the first one will be enought for this example
  NSString* interfaceName = [airportInterfaces objectAtIndex:0];

  CWInterface* airport = [CWInterface interfaceWithName:interfaceName];

  g_priorNetwork = airport.ssid;
  _LOG_DEBUG("Prior network: " << [g_priorNetwork cStringUsingEncoding:NSASCIIStringEncoding]);

  _LOG_DEBUG("Starting adhoc connection");

  NSError* error = nil;
  NSData* data = [networkName dataUsingEncoding:NSUTF8StringEncoding];
  BOOL created = [airport startIBSSModeWithSSID:data
                                       security:kCWIBSSModeSecurityNone
                                        channel:g_channel
                                       password:passphrase
                                          error:&error];

  if (!created) {
    return false;
  }

  _LOG_DEBUG("Creating face for the adhoc connection");

  // should do a better job later, when Ndnx::Control will be implemented

  ostringstream cmd;
  cmd << CCNX_PATH << "/bin/ccndc add / udp 169.254.255.255";
  int ret = system(cmd.str().c_str());
  if (ret == 0) {
    return true;
  }
  else {
    DestroyAdhoc();
    return false;
  }
}

void
Adhoc::DestroyAdhoc()
{
  NSArray* airportInterfaces = [[CWInterface interfaceNames] allObjects];

  //Choose the desired interface . the first one will be enought for this example
  NSString* interfaceName = [airportInterfaces objectAtIndex:0];

  CWInterface* airport = [CWInterface interfaceWithName:interfaceName];

  [airport disassociate];

  NSError* err;

  if (g_priorNetwork != 0) {
    NSSet* scanResults = [airport scanForNetworksWithName:g_priorNetwork error:&err];

    if ([scanResults count] > 0) {
      CWNetwork* previousNetwork = [[scanResults allObjects] objectAtIndex:0];

      [airport associateToNetwork:previousNetwork password:nil error:&err];

      g_priorNetwork = 0;
      return;
    }

    g_priorNetwork = 0;
  }

  [airport setPower:NO error:&err];
  [airport setPower:YES error:&err];

  // ok. this trick works.  if just disassociate, then it will stay OFF
  // setting power OFF/ON trick the system to reconnect to default WiFi
}

#endif // ADHOC_SUPPORTED
