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

#ifndef CHRONOSHARE_ADHOC_H
#define CHRONOSHARE_ADHOC_H

#include "config.h"

#if (__APPLE__ && HAVE_COREWLAN)
    #define ADHOC_SUPPORTED 1
#endif

#ifdef ADHOC_SUPPORTED

class Adhoc
{
public:
  static bool
  CreateAdhoc ();

  static void
  DestroyAdhoc ();
};

#endif

#endif // CHRONOSHARE_ADHOC_H
