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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#include "adhoc.h"
#include "config.h"

#if (__APPLE__ && HAVE_COREWLAN)

#include "logging.h"
#include <sstream>

using namespace std;

INIT_LOGGER ("Adhoc.OSX");

#import <CoreWLAN/CoreWLAN.h>
#import <CoreWLAN/CoreWLANConstants.h>
#import <CoreWLAN/CWInterface.h>
#import <CoreWLAN/CoreWLANTypes.h>

bool
Adhoc::CreateAdhoc ()
{
  NSString *networkName = @"NDNdirect";
  NSNumber *channel = @11;
  NSString *passphrase = @"NDNhello";
  NSString *securityMode = @"Open";

  NSArray *airportInterfaces = [CWInterface supportedInterfaces];

  //Choose the desired interface . the first one will be enought for this example
  NSString *interfaceName =[airportInterfaces objectAtIndex:0];

  CWInterface *airport = [CWInterface interfaceWithName:interfaceName];

  NSString * _priorNetwork = airport.ssid;
  _LOG_DEBUG ("Prior network: " << [_priorNetwork cStringUsingEncoding:NSASCIIStringEncoding]);

  _LOG_DEBUG ("Starting adhoc connection");

  NSError *error = nil;
  NSData* data = [networkName dataUsingEncoding:NSUTF8StringEncoding];
  BOOL created = [airport startIBSSModeWithSSID:data security:kCWIBSSModeSecurityNone channel:(NSUInteger)@11 password:passphrase error:&error];

  if (!created)
    {
      return false;
    }

  _LOG_DEBUG ("Creating face for the adhoc connection");

  // should do a better job later, when Ccnx::Control will be implemented

  ostringstream cmd;
  cmd << CCNX_PATH << "/bin/ccndc add / udp 169.254.255.255";
  int ret = system (cmd.str ().c_str ());
  if (ret == 0)
    {
      return true;
    }
  else
    {
      DestroyAdhoc ();
      return false;
    }
}

void
Adhoc::DestroyAdhoc ()
{
  NSString *networkName = @"NDNdirect";
  NSNumber *channel = @11;
  NSString *passphrase = @"NDNhello";
  NSString *securityMode = @"Open";

  NSArray *airportInterfaces = [CWInterface supportedInterfaces];

  //Choose the desired interface . the first one will be enought for this example
  NSString *interfaceName =[airportInterfaces objectAtIndex:0];

  CWInterface *airport = [CWInterface interfaceWithName:interfaceName];

  [airport disassociate];

  NSError *err;
  [airport setPower:NO error:&err];
  [airport setPower:YES error:&err];

  // ok. this trick works.  if just disassociate, then it will stay OFF
  // setting power OFF/ON trick the system to reconnect to default WiFi
}

#endif // ADHOC_SUPPORTED
