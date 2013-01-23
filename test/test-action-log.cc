/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 */

#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

#include "logging.h"
#include "action-log.h"

#include <unistd.h>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>

using namespace std;
using namespace boost;
using namespace Ccnx;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestActionLog)

BOOST_AUTO_TEST_CASE (ActionLogTest)
{
  INIT_LOGGERS ();

  Name localName ("/alex");

  fs::path tmpdir = fs::unique_path (fs::temp_directory_path () / "%%%%-%%%%-%%%%-%%%%");
  SyncLogPtr syncLog = make_shared<SyncLog> (tmpdir, localName);
  CcnxWrapperPtr ccnx = make_shared<CcnxWrapper> ();

  ActionLogPtr actionLog = make_shared<ActionLog> (ccnx, tmpdir, syncLog, "top-secret");

// const std::string &filename,
//                    const Hash &hash,
//                    time_t wtime,
//                    int mode,
//                    int seg_num
  BOOST_CHECK_EQUAL (syncLog->SeqNo (localName), 0);

  BOOST_CHECK_EQUAL (syncLog->LogSize (), 0);
  BOOST_CHECK_EQUAL (actionLog->LogSize (), 0);

  actionLog->AddLocalActionUpdate ("file.txt", *Hash::FromString ("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                              time (NULL), 0755, 10);

  BOOST_CHECK_EQUAL (syncLog->SeqNo (localName), 1);
  BOOST_CHECK_EQUAL (syncLog->LogSize (), 0);
  BOOST_CHECK_EQUAL (actionLog->LogSize (), 1);

  HashPtr hash = syncLog->RememberStateInStateLog ();
  BOOST_CHECK_EQUAL (syncLog->LogSize (), 1);
  BOOST_CHECK_EQUAL (lexical_cast<string> (*hash), "3410477233f98d6c3f9a6f8da24494bf5a65e1a7c9f4f66b228128bd4e020558");

  PcoPtr pco = actionLog->LookupActionPco (localName, 0);
  BOOST_CHECK_EQUAL ((bool)pco, false);

  pco = actionLog->LookupActionPco (localName, 1);
  BOOST_CHECK_EQUAL ((bool)pco, true);

  BOOST_CHECK_EQUAL (pco->name (), "/alex/action/top-secret/%00%01");

  ActionItemPtr action = actionLog->LookupAction (Name ("/alex/action/top-secret")(0));
  BOOST_CHECK_EQUAL ((bool)action, false);

  action = actionLog->LookupAction (Name ("/alex/action/top-secret")(1));
  BOOST_CHECK_EQUAL ((bool)action, true);

  BOOST_CHECK_EQUAL (action->version (), 0);
  BOOST_CHECK_EQUAL (action->action (), 0);

  BOOST_CHECK_EQUAL (action->filename (), "file.txt");
  BOOST_CHECK_EQUAL (action->seg_num (), 10);
  BOOST_CHECK_EQUAL (action->file_hash ().size (), 32);
  BOOST_CHECK_EQUAL (action->mode (), 0755);

  BOOST_CHECK_EQUAL (action->has_parent_device_name (), false);
  BOOST_CHECK_EQUAL (action->has_parent_seq_no (), false);

  actionLog->AddLocalActionUpdate ("file.txt", *Hash::FromString ("2ff304769cdb0125ac039e6fe7575f8576dceffc62618a431715aaf6eea2bf1c"),
                              time (NULL), 0755, 10);
  BOOST_CHECK_EQUAL (syncLog->SeqNo (localName), 2);
  BOOST_CHECK_EQUAL (syncLog->LogSize (), 1);
  BOOST_CHECK_EQUAL (actionLog->LogSize (), 2);

  action = actionLog->LookupAction (Name ("/alex"), 2);
  BOOST_CHECK_EQUAL ((bool)action, true);

  BOOST_CHECK_EQUAL (action->has_parent_device_name (), true);
  BOOST_CHECK_EQUAL (action->has_parent_seq_no (), true);

  BOOST_CHECK_EQUAL (action->parent_seq_no (), 1);
  BOOST_CHECK_EQUAL (action->version (), 1);

  BOOST_CHECK_NO_THROW (actionLog->AddRemoteAction (pco));
  BOOST_CHECK_EQUAL (actionLog->LogSize (), 2);

  // create a real remote action
  ActionItem item;
  item.set_action (ActionItem::UPDATE);
  item.set_filename ("file.txt");
  item.set_version (2);
  item.set_timestamp (time (NULL));

  BytesPtr item_msg = serializeMsg (item);
  Name actionName = Name ("/zhenkai")("action")("top-secret")(1);
  Bytes actionData = ccnx->createContentObject (actionName, head (*item_msg), item_msg->size ());

  pco = make_shared<ParsedContentObject> (actionData);
  BOOST_CHECK_NO_THROW (actionLog->AddRemoteAction (pco));
  BOOST_CHECK_EQUAL (actionLog->LogSize (), 3);

  remove_all (tmpdir);
}

BOOST_AUTO_TEST_SUITE_END()

  // catch (boost::exception &err)
  //   {
  //     cout << *boost::get_error_info<errmsg_info_str> (err) << endl;
  //   }
