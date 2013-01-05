#include "ccnx-name.h"
#include <boost/lexical_cast.hpp>
#include <ctype.h>
#include <boost/algorithm/string/join.hpp>

namespace Ccnx{
CcnxCharbufPtr CcnxCharbuf::Null;

void
CcnxCharbuf::init(ccn_charbuf *buf)
{
  if (buf != NULL)
  {
    m_buf = ccn_charbuf_create();
    ccn_charbuf_reserve(m_buf, buf->length);
    memcpy(m_buf->buf, buf->buf, buf->length);
    m_buf->length = buf->length;
  }
}

CcnxCharbuf::CcnxCharbuf()
            : m_buf(NULL)
{
  m_buf = ccn_charbuf_create();
}

CcnxCharbuf::CcnxCharbuf(ccn_charbuf *buf)
            : m_buf(NULL)
{
  init(buf);
}

CcnxCharbuf::CcnxCharbuf(const CcnxCharbuf &other)
            : m_buf (NULL)
{
  init(other.m_buf);
}

CcnxCharbuf::~CcnxCharbuf()
{
  ccn_charbuf_destroy(&m_buf);
}

Name::Name()
{
}

Name::Name(const string &name)
{
  stringstream ss(name);
  string compStr;
  bool first = true;
  while(getline(ss, compStr, '/'))
  {
    // discard the first empty comp before the first '/'
    if (first)
    {
      first = false;
      continue;
    }
    Bytes comp(compStr.begin(), compStr.end());
    m_comps.push_back(comp);
  }
}

Name::Name(const vector<Bytes> &comps)
{
  m_comps = comps;
}

Name::Name(const Name &other)
{
  m_comps = other.m_comps;
}

Name::Name(const unsigned char *data, const ccn_indexbuf *comps)
{
  for (int i = 0; i < comps->n - 1; i++)
  {
    const unsigned char *compPtr;
    size_t size;
    ccn_name_comp_get(data, comps, i, &compPtr, &size);
    Bytes comp;
    readRaw(comp, compPtr, size);
    m_comps.push_back(comp);
  }
}

Name::Name (const unsigned char *buf, const size_t length)
{
  ccn_indexbuf *idx = ccn_indexbuf_create();
  const ccn_charbuf namebuf = { length, length, const_cast<unsigned char *> (buf) };
  ccn_name_split (&namebuf, idx);

  const unsigned char *compPtr = NULL;
  size_t size = 0;
  int i = 0;
  while (ccn_name_comp_get(namebuf.buf, idx, i, &compPtr, &size) == 0)
    {
      Bytes comp;
      readRaw (comp, compPtr, size);
      m_comps.push_back(comp);
      i++;
    }
  ccn_indexbuf_destroy(&idx);
}


Name &
Name::operator=(const Name &other)
{
  m_comps = other.m_comps;
  return *this;
}
bool
Name::operator==(const string &str) const
{
  return this->toString() == str;
}

bool
Name::operator!=(const string &str) const
{
  return !(*this == str);
}

Name
operator+(const Name &n1, const Name &n2)
{
  vector<Bytes> comps = n1.m_comps;
  copy(n2.m_comps.begin(), n2.m_comps.end(), back_inserter(comps));
  return Name(comps);
}

string
Name::toString() const
{
  stringstream ss(stringstream::out);
  ss << *this;
  return ss.str();
}

CcnxCharbufPtr
Name::toCcnxCharbuf() const
{
  CcnxCharbufPtr ptr(new CcnxCharbuf());
  ccn_charbuf *cbuf = ptr->getBuf();
  ccn_name_init(cbuf);
  int size = m_comps.size();
  for (int i = 0; i < size; i++)
  {
    ccn_name_append(cbuf, head(m_comps[i]), m_comps[i].size());
  }
  return ptr;
}

Name &
Name::appendComp(const Bytes &comp)
{
  m_comps.push_back(comp);
  return *this;
}

Name &
Name::appendComp(const string &compStr)
{
  Bytes comp(compStr.begin(), compStr.end());
  appendComp(comp);
  return *this;
}

Bytes
Name::getComp(int index) const
{
  if (index >= m_comps.size())
  {
    boost::throw_exception(NameException() << error_info_str("Index out of range: " + boost::lexical_cast<string>(index)));
  }
  return m_comps[index];
}

string
Name::getCompAsString(int index) const
{
  Bytes comp = getComp(index);
  stringstream ss(stringstream::out);
  int size = comp.size();
  for (int i = 0; i < size; i++)
  {
    unsigned char ch = comp[i];
    if (isprint(ch))
    {
      ss << (char) ch;
    }
    else
    {
      ss << "%" << hex << setfill('0') << setw(2) << ch;
    }
  }

  return ss.str();
}

Name
Name::getPartialName(int start, int n) const
{
  int size = m_comps.size();
  if (start < 0 || start >= size || n > 0 && start + n > size)
  {
    stringstream ss(stringstream::out);
    ss << "getPartialName() parameter out of range! ";
    ss << "start = " << start;
    ss << "n = " << n;
    ss << "size = " << size;
    boost::throw_exception(NameException() << error_info_str(ss.str()));
  }

  vector<Bytes> comps;
  int end;
  if (n > 0)
  {
    end = start + n;
  }
  else
  {
    end = size;
  }

  for (int i = start; i < end; i++)
  {
    comps.push_back(m_comps[i]);
  }

  return Name(comps);
}

ostream &
operator <<(ostream &os, const Name &name)
{
  int size = name.size();
  vector<string> strComps;
  for (int i = 0; i < size; i++)
  {
    strComps.push_back(name.getCompAsString(i));
  }
  string joined = boost::algorithm::join(strComps, "/");
  os << "/" << joined;
  return os;
}

bool
operator ==(const Name &n1, const Name &n2)
{
  stringstream ss1(stringstream::out);
  stringstream ss2(stringstream::out);
  ss1 << n1;
  ss2 << n2;
  return ss1.str() == ss2.str();
}

bool
operator !=(const Name &n1, const Name &n2)
{
  return !(n1 == n2);
}

bool
operator <(const Name &n1, const Name &n2)
{
  stringstream ss1(stringstream::out);
  stringstream ss2(stringstream::out);
  ss1 << n1;
  ss2 << n2;
  return ss1.str() < ss2.str();
}

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
    return CcnxCharbuf::Null;
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
