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

#include "db-helper.h"
#include "logging.h"

#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/throw_exception.hpp>

INIT_LOGGER ("DbHelper");

using namespace boost;
namespace fs = boost::filesystem;

const std::string INIT_DATABASE = "\
    PRAGMA foreign_keys = ON;      \
";

DbHelper::DbHelper (const fs::path &path, const std::string &dbname)
{
  fs::create_directories (path);

  int res = sqlite3_open((path / dbname).c_str (), &m_db);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot open/create dabatabase: [" + (path / dbname).string () + "]"));
    }

  res = sqlite3_create_function (m_db, "hash", 2, SQLITE_ANY, 0, 0,
                                 DbHelper::hash_xStep, DbHelper::hash_xFinal);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``hash''"));
    }

  res = sqlite3_create_function (m_db, "is_prefix", 2, SQLITE_ANY, 0, DbHelper::is_prefix_xFun, 0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``is_prefix''"));
    }

  res = sqlite3_create_function (m_db, "directory_name", -1, SQLITE_ANY, 0, DbHelper::directory_name_xFun, 0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``directory_name''"));
    }

  res = sqlite3_create_function (m_db, "is_dir_prefix", 2, SQLITE_ANY, 0, DbHelper::is_dir_prefix_xFun, 0, 0);
  if (res != SQLITE_OK)
    {
      BOOST_THROW_EXCEPTION (Error::Db ()
                             << errmsg_info_str ("Cannot create function ``is_dir_prefix''"));
    }

  sqlite3_exec (m_db, INIT_DATABASE.c_str (), NULL, NULL, NULL);
  _LOG_DEBUG_COND (sqlite3_errcode (m_db) != SQLITE_OK, sqlite3_errmsg (m_db));
}

DbHelper::~DbHelper ()
{
  int res = sqlite3_close (m_db);
  if (res != SQLITE_OK)
    {
      // complain
    }
}

void
DbHelper::hash_xStep (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 2)
    {
      // _LOG_ERROR ("Wrong arguments are supplied for ``hash'' function");
      sqlite3_result_error (context, "Wrong arguments are supplied for ``hash'' function", -1);
      return;
    }
  if (sqlite3_value_type (argv[0]) != SQLITE_BLOB ||
      sqlite3_value_type (argv[1]) != SQLITE_INTEGER)
    {
      // _LOG_ERROR ("Hash expects (blob,integer) parameters");
      sqlite3_result_error (context, "Hash expects (blob,integer) parameters", -1);
      return;
    }

  EVP_MD_CTX **hash_context = reinterpret_cast<EVP_MD_CTX **> (sqlite3_aggregate_context (context, sizeof (EVP_MD_CTX *)));

  if (hash_context == 0)
    {
      sqlite3_result_error_nomem (context);
      return;
    }

  if (*hash_context == 0)
    {
      *hash_context = EVP_MD_CTX_create ();
      EVP_DigestInit_ex (*hash_context, HASH_FUNCTION (), 0);
    }

  int nameBytes       = sqlite3_value_bytes (argv[0]);
  const void *name    = sqlite3_value_blob  (argv[0]);
  sqlite3_int64 seqno = sqlite3_value_int64 (argv[1]);

  EVP_DigestUpdate (*hash_context, name, nameBytes);
  EVP_DigestUpdate (*hash_context, &seqno, sizeof(sqlite3_int64));
}

void
DbHelper::hash_xFinal (sqlite3_context *context)
{
  EVP_MD_CTX **hash_context = reinterpret_cast<EVP_MD_CTX **> (sqlite3_aggregate_context (context, sizeof (EVP_MD_CTX *)));

  if (hash_context == 0)
    {
      sqlite3_result_error_nomem (context);
      return;
    }

  if (*hash_context == 0) // no rows
    {
      char charNullResult = 0;
      sqlite3_result_blob (context, &charNullResult, 1, SQLITE_TRANSIENT); //SQLITE_TRANSIENT forces to make a copy
      return;
    }

  unsigned char *hash = new unsigned char [EVP_MAX_MD_SIZE];
  unsigned int hashLength = 0;

  int ok = EVP_DigestFinal_ex (*hash_context,
			       hash, &hashLength);

  sqlite3_result_blob (context, hash, hashLength, SQLITE_TRANSIENT); //SQLITE_TRANSIENT forces to make a copy
  delete [] hash;

  EVP_MD_CTX_destroy (*hash_context);
}

void
DbHelper::is_prefix_xFun (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  int len1 = sqlite3_value_bytes (argv[0]);
  int len2 = sqlite3_value_bytes (argv[1]);

  if (len1 == 0)
    {
      sqlite3_result_int (context, 1);
      return;
    }

  if (len1 > len2) // first parameter should be at most equal in length to the second one
    {
      sqlite3_result_int (context, 0);
      return;
    }

  if (memcmp (sqlite3_value_blob (argv[0]), sqlite3_value_blob (argv[1]), len1) == 0)
    {
      sqlite3_result_int (context, 1);
    }
  else
    {
      sqlite3_result_int (context, 0);
    }
}

void
DbHelper::directory_name_xFun (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  if (argc != 1)
    {
      sqlite3_result_error (context, "``directory_name'' expects 1 text argument", -1);
      sqlite3_result_null (context);
      return;
    }

  if (sqlite3_value_bytes (argv[0]) == 0)
    {
      sqlite3_result_null (context);
      return;
    }

  boost::filesystem::path filePath (std::string (reinterpret_cast<const char*> (sqlite3_value_text (argv[0])), sqlite3_value_bytes (argv[0])));
  std::string dirPath = filePath.parent_path ().generic_string ();
  // _LOG_DEBUG ("directory_name FUN: " << dirPath);
  if (dirPath.size () == 0)
    {
      sqlite3_result_null (context);
    }
  else
    {
      sqlite3_result_text (context, dirPath.c_str (), dirPath.size (), SQLITE_TRANSIENT);
    }
}

void
DbHelper::is_dir_prefix_xFun (sqlite3_context *context, int argc, sqlite3_value **argv)
{
  int len1 = sqlite3_value_bytes (argv[0]);
  int len2 = sqlite3_value_bytes (argv[1]);

  if (len1 == 0)
    {
      sqlite3_result_int (context, 1);
      return;
    }

  if (len1 > len2) // first parameter should be at most equal in length to the second one
    {
      sqlite3_result_int (context, 0);
      return;
    }

  if (memcmp (sqlite3_value_blob (argv[0]), sqlite3_value_blob (argv[1]), len1) == 0)
    {
      if (len1 == len2)
        {
          sqlite3_result_int (context, 1);
        }
      else
        {
          if (reinterpret_cast<const char*> (sqlite3_value_blob (argv[1]))[len1] == '/')
            {
              sqlite3_result_int (context, 1);
            }
          else
            {
              sqlite3_result_int (context, 0);
            }
        }
    }
  else
    {
      sqlite3_result_int (context, 0);
    }
}
