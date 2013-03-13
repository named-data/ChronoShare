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
#include "ccnx-cert.h"
#include <tinyxml.h>
#include <boost/lexical_cast.hpp>
#include "logging.h"

INIT_LOGGER ("Ccnx.Cert");

using namespace std;

namespace Ccnx {

Cert::Cert()
    : m_meta("", "",  0, 0)
{
}

Cert::Cert(const PcoPtr &keyObject, const PcoPtr &metaObject = PcoPtr())
    : m_meta("", "", 0, 0)
{
  m_name = keyObject->name();
  m_raw = keyObject->content();
  m_hash = *(Hash::FromBytes(m_raw));
  updateMeta(metaObject);
}

void
Cert::updateMeta(const PcoPtr &metaObject)
{
  if (metaObject)
  {
    TiXmlDocument doc;
    Bytes xml = metaObject->content();
    // just make sure it's null terminated as it's required by TiXmlDocument::parse
    xml.push_back('\0');
    doc.Parse((const char *)(head(xml)));
    if (!doc.Error())
    {
      TiXmlElement *root = doc.RootElement();
      for (TiXmlElement *child = root->FirstChildElement(); child; child = child->NextSiblingElement())
      {
        string elemName = child->Value();
        string text = child->GetText();
        if (elemName == "Name")
        {
          m_meta.realworldID = text;
        }
        else if (elemName == "Affiliation")
        {
          m_meta.affiliation = text;
        }
        else if (elemName == "Valid_to")
        {
          m_meta.validTo = boost::lexical_cast<time_t>(text);
        }
        else if (elemName == "Valid_from")
        {
          // this is not included in the key meta yet
          // but it should eventually be there
        }
        else
        {
          // ignore known stuff
        }
      }
    }
    else
    {
      _LOG_ERROR("Cannot parse meta info:" << std::string(head(xml), xml.size()));
    }
  }
}

Cert::VALIDITY
Cert::validity()
{
  if (m_meta.validFrom == 0 && m_meta.validTo == 0)
  {
    return OTHER;
  }

  time_t now = time(NULL);
  if (now < m_meta.validFrom)
  {
    return NOT_YET_VALID;
  }

  if (now >= m_meta.validTo)
  {
    return EXPIRED;
  }

  return WITHIN_VALID_TIME_SPAN;
}

} // Ccnx
