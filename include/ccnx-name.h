#ifndef CCNX_NAME_H
#define CCNX_NAME_H
#include <boost/shared_ptr.hpp>
#include "ccnx-common.h"

namespace Ccnx {

class CcnxCharbuf;
typedef boost::shared_ptr<CcnxCharbuf> CcnxCharbufPtr;

//  This class is mostly used in CcnxWrapper; users should not be directly using this class
// The main purpose of this class to is avoid manually create and destroy charbuf everytime
class CcnxCharbuf
{
public:
  CcnxCharbuf();
  CcnxCharbuf(ccn_charbuf *buf);
  ~CcnxCharbuf();

  // expose internal data structure, use with caution!!
  ccn_charbuf *
  getBuf() { return m_buf; }
  static CcnxCharbufPtr Null;

protected:
  ccn_charbuf *m_buf;
};


struct NameException:
  virtual boost::exception, virtual exception {};

class Name
{
public:
  Name();
  Name(const string &name);
  Name(const vector<Bytes> &comps);
  Name(const Name &other);
  Name(const unsigned char *data, const ccn_indexbuf *comps);
  virtual ~Name() {}

  CcnxCharbufPtr
  toCcnxCharbuf() const;

  Name &
  appendComp(const Bytes &comp);

  Name &
  appendComp(const string &compStr);

  int
  size() const {return m_comps.size();}

  Bytes
  getComp(int index) const;

  // return string format of the comp
  // if all characters are printable, simply returns the string
  // if not, print the bytes in hex string format
  string
  getCompAsString(int index) const;

  Name
  getPartialName(int start, int n = -1) const;

  string
  toString() const;

  Name &
  operator=(const Name &other);

  bool
  operator==(const string &str);

  bool
  operator!=(const string &str);

  friend Name
  operator+(const Name &n1, const Name &n2);

protected:
  vector<Bytes> m_comps;
};

ostream&
operator <<(ostream &os, const Name &name);

bool
operator ==(const Name &n1, const Name &n2);

bool
operator !=(const Name &n1, const Name &n2);

bool
operator <(const Name &n1, const Name &n2);


struct InterestSelectorException:
  virtual boost::exception, virtual exception {};

class Selectors
{
public:
  Selectors();
  Selectors(const Selectors &other);
  ~Selectors(){};

  typedef enum
  {
    AOK_CS = 0x1,
    AOK_NEW = 0x2,
    AOK_DEFAULT = 0x3, // (AOK_CS | AOK_NEW)
    AOK_STALE = 0x4,
    AOK_EXPIRE = 0x10
  } AOK;

  typedef enum
  {
    LEFT = 0,
    RIGHT = 1,
    DEFAULT = 2
  } CHILD_SELECTOR;

  inline Selectors &
  maxSuffixComps(int maxSCs) {m_maxSuffixComps = maxSCs; return *this;}

  inline Selectors &
  minSuffixComps(int minSCs) {m_minSuffixComps = minSCs; return *this;}

  inline Selectors &
  answerOriginKind(AOK kind) {m_answerOriginKind = kind; return *this;}

  inline Selectors &
  interestLifetime(int lifetime) {m_interestLifetime = lifetime; return *this;}

  inline Selectors &
  scope(int scope) {m_scope = scope; return *this;}

  inline Selectors &
  childSelector(CHILD_SELECTOR child) {m_childSelector = child; return *this;}

  // this has no effect now
  inline Selectors &
  publisherPublicKeyDigest(const Bytes &digest) {m_publisherPublicKeyDigest = digest; return *this;}

  CcnxCharbufPtr
  toCcnxCharbuf() const;

  bool
  isEmpty() const;

  bool
  operator==(const Selectors &other);

private:
  int m_maxSuffixComps;
  int m_minSuffixComps;
  AOK m_answerOriginKind;
  double m_interestLifetime;
  int m_scope;
  CHILD_SELECTOR m_childSelector;
  // not used now
  Bytes m_publisherPublicKeyDigest;
};

} // Ccnx
#endif
