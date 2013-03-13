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

#include "ccnx-verifier.h"

namespace Ccnx {

static const size_t ROOT_KEY_DIGEST_LEN = 32;  // SHA-256
static const unsigned char ROOT_KEY_DIGEST[ROOT_KEY_DIGEST_LEN] = {0xa7, 0xd9, 0x8b, 0x81, 0xde, 0x13, 0xfc,
0x56, 0xc5, 0xa6, 0x92, 0xb4, 0x44, 0x93, 0x6e, 0x56, 0x70, 0x9d, 0x52, 0x6f, 0x70,
0xed, 0x39, 0xef, 0xb5, 0xe2, 0x3, 0x29, 0xa5, 0x53, 0x3e, 0x68};

Verifier::Verifier(CcnxWrapper *ccnx)
         : m_ccnx(ccnx)
         , m_rootKeyDigest(ROOT_KEY_DIGEST, ROOT_KEY_DIGEST_LEN)
{
}

Verifier::~Verifier()
{
}

bool
Verifier::verify(const PcoPtr &pco)
{
  if (pco->integrityChecked())
  {
    return false;
  }

  HashPtr publisherPublicKeyDigest = pco->publisherPublicKeyDigest();
  KeyCache::iterator it = m_keyCache.find(*publisherPublicKeyDigest);
  if (it != m_keyCache.end())
  {
    KeyPtr key = it->second;
    if (key->validity() == Key::WITHIN_VALID_TIME_SPAN)
    {
      // integrity checked, and the key is trustworthy
      pco->setVerified(true);
      return true;
    }
    else
    {
      // delete the invalid key cache
      m_keyCache.erase(it);
    }
  }

  // keyName is the name specified in key locator, i.e. without version and segment
  Name keyName = pco->keyName();
  int keyNameSize = keyName.size();

  // for keys, we have to make sure key name is strictly prefix of the content name
  if (pco->type() == ParsedContentObject::KEY)
  {
    Name contentName = pco->name();
    if (keyNameSize >= contentName.size() || contentName.getPartialName(0, keyNameSize) != keyName)
    {
      return false;
    }
  }
  else
  {
    // for now, user can assign any data using his key
  }

  Name metaName = keyName.getPartialName(0, keyNameSize - 1) + Name("/info") + keyName.getPartialName(keyNameSize - 1);

  Selectors selectors;

  selectors.childSelector(Selectors::RIGHT)
           .interestLifetime(1.0);

  PcoPtr keyObject = m_ccnx->get(keyName, selectors);
  PcoPtr metaObject = m_ccnx->get(metaName, selectors);
  if (!keyObject || !metaObject || !keyObject->integrityChecked() || !metaObject->integrityChecked())
  {
    return false;
  }

  HashPtr publisherKeyHashInKeyObject = keyObject->publisherPublicKeyDigest();
  HashPtr publisherKeyHashInMetaObject = metaObject->publisherPublicKeyDigest();

  // make sure key and meta are signed using the same key
  if (publisherKeyHashInKeyObject->IsZero() || ! (*publisherKeyHashInKeyObject == *publisherKeyHashInMetaObject))
  {
    return false;
  }

  KeyPtr key = boost::make_shared<Key>(keyObject, metaObject);
  if (key->validity() != Key::WITHIN_VALID_TIME_SPAN)
  {
    return false;
  }

  // check pco is actually signed by this key (maybe redundant)
  if (! (*pco->publisherPublicKeyDigest() == key->hash()))
  {
    return false;
  }

  // now we only need to make sure the keyObject is trustworthy
  if (key->hash() == m_rootKeyDigest)
  {
    // the key is the root key
    // do nothing now
  }
  else
  {
    // can not verify key
    if (!verify(keyObject))
    {
      return false;
    }
  }

  // ok, keyObject verified, because metaObject is signed by the same parent key and integrity checked
  // so metaObject is also verified
  m_keyCache.insert(std::make_pair(key->hash(), key));

  pco->setVerified(true);
  return true;
}

} // Ccnx
