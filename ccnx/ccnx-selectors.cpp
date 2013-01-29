#include "ccnx-selectors.h"
#include "ccnx-common.h"
#include <boost/lexical_cast.hpp>

using namespace std;

namespace Ccnx {

Selectors::Selectors()
          : m_maxSuffixComps(-1)
          , m_minSuffixComps(-1)
          , m_answerOriginKind(AOK_DEFAULT)
          , m_interestLifetime(-1.0)
          , m_scope(-1)
          , m_childSelector(DEFAULT)
{
}

Selectors::Selectors(const Selectors &other)
{
  m_maxSuffixComps = other.m_maxSuffixComps;
  m_minSuffixComps = other.m_minSuffixComps;
  m_answerOriginKind = other.m_answerOriginKind;
  m_interestLifetime = other.m_interestLifetime;
  m_scope = other.m_scope;
  m_childSelector = other.m_childSelector;
  m_publisherPublicKeyDigest = other.m_publisherPublicKeyDigest;
}

Selectors::Selectors(const ccn_parsed_interest *pi)
          : m_maxSuffixComps(-1)
          , m_minSuffixComps(-1)
          , m_answerOriginKind(AOK_DEFAULT)
          , m_interestLifetime(-1.0)
          , m_scope(-1)
          , m_childSelector(DEFAULT)
{
  if (pi != NULL)
  {
    m_maxSuffixComps = pi->max_suffix_comps;
    m_minSuffixComps = pi->min_suffix_comps;
    switch(pi->orderpref)
    {
      case 0: m_childSelector = LEFT; break;
      case 1: m_childSelector = RIGHT; break;
      default: break;
    }
    switch(pi->answerfrom)
    {
      case 0x1: m_answerOriginKind = AOK_CS; break;
      case 0x2: m_answerOriginKind = AOK_NEW; break;
      case 0x3: m_answerOriginKind = AOK_DEFAULT; break;
      case 0x4: m_answerOriginKind = AOK_STALE; break;
      case 0x10: m_answerOriginKind = AOK_EXPIRE; break;
      default: break;
    }
    m_scope = pi->scope;
    // scope and interest lifetime do not really matter to receiving application, it's only meaningful to routers
  }
}

bool
Selectors::operator == (const Selectors &other)
{
  return m_maxSuffixComps == other.m_maxSuffixComps
         && m_minSuffixComps == other.m_minSuffixComps
         && m_answerOriginKind == other.m_answerOriginKind
         && (m_interestLifetime - other.m_interestLifetime) < 10e-4
         && m_scope == other.m_scope
         && m_childSelector == other.m_childSelector;
}

bool
Selectors::isEmpty() const
{
  return m_maxSuffixComps == -1
         && m_minSuffixComps == -1
         && m_answerOriginKind == AOK_DEFAULT
         && (m_interestLifetime - (-1.0)) < 10e-4
         && m_scope == -1
         && m_childSelector == DEFAULT;
}


CcnxCharbufPtr
Selectors::toCcnxCharbuf() const
{
  if (isEmpty())
  {
    return CcnxCharbufPtr ();
  }
  CcnxCharbufPtr ptr(new CcnxCharbuf());
  ccn_charbuf *cbuf = ptr->getBuf();
  ccn_charbuf_append_tt(cbuf, CCN_DTAG_Interest, CCN_DTAG);
  ccn_charbuf_append_tt(cbuf, CCN_DTAG_Name, CCN_DTAG);
  ccn_charbuf_append_closer(cbuf); // </Name>

  if (m_maxSuffixComps < m_minSuffixComps)
  {
    boost::throw_exception(InterestSelectorException() << error_info_str("MaxSuffixComps = " + boost::lexical_cast<string>(m_maxSuffixComps) + " is smaller than  MinSuffixComps = " + boost::lexical_cast<string>(m_minSuffixComps)));
  }

  if (m_maxSuffixComps > 0)
  {
    ccnb_tagged_putf(cbuf, CCN_DTAG_MaxSuffixComponents, "%d", m_maxSuffixComps);
  }

  if (m_minSuffixComps > 0)
  {
    ccnb_tagged_putf(cbuf, CCN_DTAG_MinSuffixComponents, "%d", m_minSuffixComps);
  }

  if (m_answerOriginKind != AOK_DEFAULT)
  {
    // it was not using "ccnb_tagged_putf" in ccnx c code, no idea why
    ccn_charbuf_append_tt(cbuf, CCN_DTAG_AnswerOriginKind, CCN_DTAG);
    ccnb_append_number(cbuf, m_answerOriginKind);
    ccn_charbuf_append_closer(cbuf); // <AnswerOriginKind>
  }

  if (m_scope != -1)
  {
    ccnb_tagged_putf(cbuf, CCN_DTAG_Scope, "%d", m_scope);
  }

  if (m_interestLifetime > 0.0)
  {
    // Ccnx timestamp unit is weird 1/4096 second
    // this is from their code
    unsigned lifetime = 4096 * (m_interestLifetime + 1.0/8192.0);
    if (lifetime == 0 || lifetime > (30 << 12))
    {
      boost::throw_exception(InterestSelectorException() << error_info_str("Ccnx requires 0 < lifetime < 30.0. lifetime= " + boost::lexical_cast<string>(m_interestLifetime)));
    }
    unsigned char buf[3] = {0};
    for (int i = sizeof(buf) - 1; i >= 0; i--, lifetime >>= 8)
    {
      buf[i] = lifetime & 0xff;
    }
    ccnb_append_tagged_blob(cbuf, CCN_DTAG_InterestLifetime, buf, sizeof(buf));
  }

  if (m_childSelector != DEFAULT)
  {
    ccnb_tagged_putf(cbuf, CCN_DTAG_ChildSelector, "%d", m_childSelector);
  }


  ccn_charbuf_append_closer(cbuf); // </Interest>

  return ptr;
}

} // Ccnx
