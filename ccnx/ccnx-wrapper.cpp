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

#include "ccnx-wrapper.h"
extern "C" {
#include <ccn/fetch.h>
}
#include <poll.h>
#include <boost/throw_exception.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>

#include "ccnx-verifier.h"
#include "logging.h"

INIT_LOGGER ("Ccnx.Wrapper");

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;
typedef boost::error_info<struct tag_errmsg, int> errmsg_info_int;

using namespace std;
using namespace boost;

namespace Ccnx {

// hack to enable fake signatures
// min length for signature field is 16, as defined in ccn_buf_decoder.c:728
const int DEFAULT_SIGNATURE_SIZE = 16;

// Although ccn_buf_decoder.c:745 defines minimum length 16, something else is checking and only 32-byte fake value is accepted by ccnd
const int PUBLISHER_KEY_SIZE = 32;

static int
ccn_encode_garbage_Signature(struct ccn_charbuf *buf)
{
    int res = 0;

    res |= ccn_charbuf_append_tt(buf, CCN_DTAG_Signature, CCN_DTAG);

    // Let's cheat more.  Default signing algorithm in ccnd is SHA256, so we just need add 32 bytes of garbage
    static char garbage [DEFAULT_SIGNATURE_SIZE];

    // digest and witness fields are optional, so use default ones

    res |= ccn_charbuf_append_tt(buf, CCN_DTAG_SignatureBits, CCN_DTAG);
    res |= ccn_charbuf_append_tt(buf, DEFAULT_SIGNATURE_SIZE, CCN_BLOB);
    res |= ccn_charbuf_append(buf, garbage, DEFAULT_SIGNATURE_SIZE);
    res |= ccn_charbuf_append_closer(buf);

    res |= ccn_charbuf_append_closer(buf);

    return(res == 0 ? 0 : -1);
}

static int
ccn_pack_unsigned_ContentObject(struct ccn_charbuf *buf,
                                const struct ccn_charbuf *Name,
                                const struct ccn_charbuf *SignedInfo,
                                const void *data,
                                size_t size)
{
    int res = 0;
    struct ccn_charbuf *content_header;
    size_t closer_start;

    content_header = ccn_charbuf_create();
    res |= ccn_charbuf_append_tt(content_header, CCN_DTAG_Content, CCN_DTAG);
    if (size != 0)
        res |= ccn_charbuf_append_tt(content_header, size, CCN_BLOB);
    closer_start = content_header->length;
    res |= ccn_charbuf_append_closer(content_header);
    if (res < 0)
        return(-1);

    res |= ccn_charbuf_append_tt(buf, CCN_DTAG_ContentObject, CCN_DTAG);

    res |= ccn_encode_garbage_Signature(buf);

    res |= ccn_charbuf_append_charbuf(buf, Name);
    res |= ccn_charbuf_append_charbuf(buf, SignedInfo);
    res |= ccnb_append_tagged_blob(buf, CCN_DTAG_Content, data, size);
    res |= ccn_charbuf_append_closer(buf);

    ccn_charbuf_destroy(&content_header);
    return(res == 0 ? 0 : -1);
}

CcnxWrapper::CcnxWrapper()
  : m_handle (0)
  , m_running (true)
  , m_connected (false)
  , m_executor (new Executor(1))
  , m_verifier(new Verifier(this))
{
  start ();
}

void
CcnxWrapper::connectCcnd()
{
  if (m_handle != 0) {
    ccn_disconnect (m_handle);
    //ccn_destroy (&m_handle);
  }
  else
    {
      m_handle = ccn_create ();
    }

  UniqueRecLock lock(m_mutex);
  if (ccn_connect(m_handle, NULL) < 0)
  {
    BOOST_THROW_EXCEPTION (CcnxOperationException() << errmsg_info_str("connection to ccnd failed"));
  }
  m_connected = true;

  if (!m_registeredInterests.empty())
  {
   for (map<Name, InterestCallback>::const_iterator it = m_registeredInterests.begin(); it != m_registeredInterests.end(); ++it)
    {
      clearInterestFilter(it->first, false);
      setInterestFilter(it->first, it->second, false);
    }
  }
}

CcnxWrapper::~CcnxWrapper()
{
  shutdown ();
  if (m_verifier != 0)
  {
    delete m_verifier;
    m_verifier = 0;
  }
}

void
CcnxWrapper::start () // called automatically in constructor
{
  connectCcnd();
  m_thread = thread (&CcnxWrapper::ccnLoop, this);
  m_executor->start();
}

void
CcnxWrapper::shutdown () // called in destructor, but can called manually
{
  m_executor->shutdown();

  {
    UniqueRecLock lock(m_mutex);
    m_running = false;
  }

  _LOG_DEBUG ("+++++++++SHUTDOWN+++++++");
  if (m_connected)
    {
      m_thread.join ();

      ccn_disconnect (m_handle);
      //ccn_destroy (&m_handle);
      m_connected = false;
    }
}

void
CcnxWrapper::ccnLoop ()
{
  static boost::mt19937 randomGenerator (static_cast<unsigned int> (std::time (0)));
  static boost::variate_generator<boost::mt19937&, boost::uniform_int<> > rangeUniformRandom (randomGenerator, uniform_int<> (0,1000));

  while (m_running)
    {
      try
        {
          int res = 0;
          {
            UniqueRecLock lock(m_mutex);
            res = ccn_run (m_handle, 0);
          }

          if (!m_running) break;

          if (res < 0) {
            _LOG_ERROR ("ccn_run returned negative status: " << res);

            BOOST_THROW_EXCEPTION (CcnxOperationException()
                                   << errmsg_info_str("ccn_run returned error"));
          }


          pollfd pfds[1];
          {
            UniqueRecLock lock(m_mutex);

            pfds[0].fd = ccn_get_connection_fd (m_handle);
            pfds[0].events = POLLIN;
            if (ccn_output_is_pending (m_handle))
              pfds[0].events |= POLLOUT;
          }

          int ret = poll (pfds, 1, 1);
          if (ret < 0)
            {
              BOOST_THROW_EXCEPTION (CcnxOperationException() << errmsg_info_str("ccnd socket failed (probably ccnd got stopped)"));
            }
        }
        catch (CcnxOperationException &e)
        {
          m_connected = false;
          // probably ccnd has been stopped
          // try reconnect with sleep
          int interval = 1;
          int maxInterval = 32;
          while (m_running)
          {
            try
            {
              this_thread::sleep (boost::get_system_time () +  boost::posix_time::seconds (interval) + boost::posix_time::milliseconds (rangeUniformRandom ()));

              connectCcnd ();
              _LOG_DEBUG("reconnect to ccnd succeeded");
              break;
            }
            catch (CcnxOperationException &e)
            {
              this_thread::sleep (boost::get_system_time () +  boost::posix_time::seconds (interval) + boost::posix_time::milliseconds (rangeUniformRandom ()));

              // do exponential backup for reconnect interval
              if (interval < maxInterval)
              {
                interval *= 2;
              }
            }
          }
        }
        catch (const std::exception &exc)
          {
            // catch anything thrown within try block that derives from std::exception
            std::cerr << exc.what();
          }
        catch (...)
          {
            cout << "UNKNOWN EXCEPTION !!!" << endl;
          }
     }
}

Bytes
CcnxWrapper::createContentObject(const Name  &name, const void *buf, size_t len, int freshness, const Name &keyNameParam)
{
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_TRACE ("<< not running or connected");
        return Bytes ();
      }
  }

  CcnxCharbufPtr ptr = name.toCcnxCharbuf();
  ccn_charbuf *pname = ptr->getBuf();
  ccn_charbuf *content = ccn_charbuf_create();

  struct ccn_signing_params sp = CCN_SIGNING_PARAMS_INIT;
  sp.freshness = freshness;

  Name keyName;

  if (keyNameParam.size() == 0)
  {
    // use default key name
    CcnxCharbufPtr defaultKeyNamePtr = boost::make_shared<CcnxCharbuf>();
    ccn_get_public_key_and_name(m_handle, &sp, NULL, NULL, defaultKeyNamePtr->getBuf());
    keyName = Name(*defaultKeyNamePtr);
  }
  else
  {
    keyName = keyNameParam;
  }

  if (sp.template_ccnb == NULL)
  {
    sp.template_ccnb = ccn_charbuf_create();
    ccn_charbuf_append_tt(sp.template_ccnb, CCN_DTAG_SignedInfo, CCN_DTAG);
  }
  // no idea what the following 3 lines do, but it was there
  else if (sp.template_ccnb->length > 0) {
      sp.template_ccnb->length--;
  }
  ccn_charbuf_append_tt(sp.template_ccnb, CCN_DTAG_KeyLocator, CCN_DTAG);
  ccn_charbuf_append_tt(sp.template_ccnb, CCN_DTAG_KeyName, CCN_DTAG);
  CcnxCharbufPtr keyPtr = keyName.toCcnxCharbuf();
  ccn_charbuf *keyBuf = keyPtr->getBuf();
  ccn_charbuf_append(sp.template_ccnb, keyBuf->buf, keyBuf->length);
  ccn_charbuf_append_closer(sp.template_ccnb); // </KeyName>
  ccn_charbuf_append_closer(sp.template_ccnb); // </KeyLocator>
  sp.sp_flags |= CCN_SP_TEMPL_KEY_LOCATOR;
  ccn_charbuf_append_closer(sp.template_ccnb); // </SignedInfo>

  if (ccn_sign_content(m_handle, content, pname, &sp, buf, len) != 0)
  {
    BOOST_THROW_EXCEPTION(CcnxOperationException() << errmsg_info_str("sign content failed"));
  }

  Bytes bytes;
  readRaw(bytes, content->buf, content->length);

  ccn_charbuf_destroy (&content);
  if (sp.template_ccnb != NULL)
  {
    ccn_charbuf_destroy (&sp.template_ccnb);
  }

  return bytes;
}

int
CcnxWrapper::putToCcnd (const Bytes &contentObject)
{
  _LOG_TRACE (">> putToCcnd");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
    {
      _LOG_TRACE ("<< not running or connected");
      return -1;
    }


  if (ccn_put(m_handle, head(contentObject), contentObject.size()) < 0)
  {
    _LOG_ERROR ("ccn_put failed");
    // BOOST_THROW_EXCEPTION(CcnxOperationException() << errmsg_info_str("ccnput failed"));
  }
  else
    {
      _LOG_DEBUG ("<< putToCcnd");
    }

  return 0;
}

int
CcnxWrapper::publishData (const Name &name, const unsigned char *buf, size_t len, int freshness, const Name &keyName)
{
  Bytes co = createContentObject(name, buf, len, freshness, keyName);
  return putToCcnd(co);
}

int
CcnxWrapper::publishUnsignedData(const Name &name, const unsigned char *buf, size_t len, int freshness)
{
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_TRACE ("<< not running or connected");
        return -1;
      }
  }

  CcnxCharbufPtr ptr = name.toCcnxCharbuf();
  ccn_charbuf *pname = ptr->getBuf();
  ccn_charbuf *content = ccn_charbuf_create();
  ccn_charbuf *signed_info = ccn_charbuf_create();

  static char fakeKey[PUBLISHER_KEY_SIZE];

  int res = ccn_signed_info_create(signed_info,
                                   fakeKey, PUBLISHER_KEY_SIZE,
                                   NULL,
                                   CCN_CONTENT_DATA,
                                   freshness,
                                   NULL,
                                   NULL  // ccnd is happy with absent key locator and key itself... ha ha
                                   );
  ccn_pack_unsigned_ContentObject(content, pname, signed_info, buf, len);

  Bytes bytes;
  readRaw(bytes, content->buf, content->length);

  ccn_charbuf_destroy (&content);
  ccn_charbuf_destroy (&signed_info);

  return putToCcnd (bytes);
}


static void
deleterInInterestTuple (tuple<CcnxWrapper::InterestCallback *, ExecutorPtr> *tuple)
{
  delete tuple->get<0> ();
  delete tuple;
}

static ccn_upcall_res
incomingInterest(ccn_closure *selfp,
                 ccn_upcall_kind kind,
                 ccn_upcall_info *info)
{
  CcnxWrapper::InterestCallback *f;
  ExecutorPtr executor;
  tuple<CcnxWrapper::InterestCallback *, ExecutorPtr> *realData = reinterpret_cast< tuple<CcnxWrapper::InterestCallback *, ExecutorPtr>* > (selfp->data);
  tie (f, executor) = *realData;

  switch (kind)
    {
    case CCN_UPCALL_FINAL: // effective in unit tests
      // delete closure;
      executor->execute (bind (deleterInInterestTuple, realData));

      delete selfp;
      _LOG_TRACE ("<< incomingInterest with CCN_UPCALL_FINAL");
      return CCN_UPCALL_RESULT_OK;

    case CCN_UPCALL_INTEREST:
      _LOG_TRACE (">> incomingInterest upcall: " << Name(info->interest_ccnb, info->interest_comps));
      break;

    default:
      _LOG_TRACE ("<< incomingInterest with CCN_UPCALL_RESULT_OK: " << Name(info->interest_ccnb, info->interest_comps));
      return CCN_UPCALL_RESULT_OK;
    }

  Name interest(info->interest_ccnb, info->interest_comps);
  Selectors selectors(info->pi);

  executor->execute (bind (*f, interest, selectors));
  // this will be run in executor
  // (*f) (interest);
  // closure->runInterestCallback(interest);

  return CCN_UPCALL_RESULT_OK;
}

static void
deleterInDataTuple (tuple<Closure *, ExecutorPtr, Selectors> *tuple)
{
  delete tuple->get<0> ();
  delete tuple;
}

static ccn_upcall_res
incomingData(ccn_closure *selfp,
             ccn_upcall_kind kind,
             ccn_upcall_info *info)
{
  // Closure *cp = static_cast<Closure *> (selfp->data);
  Closure *cp;
  ExecutorPtr executor;
  Selectors selectors;
  tuple<Closure *, ExecutorPtr, Selectors> *realData = reinterpret_cast< tuple<Closure*, ExecutorPtr, Selectors>* > (selfp->data);
  tie (cp, executor, selectors) = *realData;

  switch (kind)
    {
    case CCN_UPCALL_FINAL:  // effecitve in unit tests
      executor->execute (bind (deleterInDataTuple, realData));

      cp = NULL;
      delete selfp;
      _LOG_TRACE ("<< incomingData with CCN_UPCALL_FINAL");
      return CCN_UPCALL_RESULT_OK;

    case CCN_UPCALL_CONTENT:
      _LOG_TRACE (">> incomingData content upcall: " << Name (info->content_ccnb, info->content_comps));
      break;

    // this is the case where the intentionally unsigned packets coming (in Encapsulation case)
    case CCN_UPCALL_CONTENT_BAD:
      _LOG_TRACE (">> incomingData content bad upcall: " << Name (info->content_ccnb, info->content_comps));
      break;

    // always ask ccnd to try to fetch the key
    case CCN_UPCALL_CONTENT_UNVERIFIED:
      _LOG_TRACE (">> incomingData content unverified upcall: " << Name (info->content_ccnb, info->content_comps));
      break;

    case CCN_UPCALL_INTEREST_TIMED_OUT: {
      if (cp != NULL)
      {
        Name interest(info->interest_ccnb, info->interest_comps);
        _LOG_TRACE ("<< incomingData timeout: " << Name (info->interest_ccnb, info->interest_comps));
        executor->execute (bind (&Closure::runTimeoutCallback, cp, interest, *cp, selectors));
      }
      else
        {
          _LOG_TRACE ("<< incomingData timeout, but callback is not set...: " << Name (info->interest_ccnb, info->interest_comps));
        }
      return CCN_UPCALL_RESULT_OK;
    }

    default:
      _LOG_TRACE(">> unknown upcall type");
      return CCN_UPCALL_RESULT_OK;
    }

  PcoPtr pco = make_shared<ParsedContentObject> (info->content_ccnb, info->pco->offset[CCN_PCO_E]);

  // this will be run in executor
  executor->execute (bind (&Closure::runDataCallback, cp, pco->name (), pco));
  _LOG_TRACE (">> incomingData");

  return CCN_UPCALL_RESULT_OK;
}

int CcnxWrapper::sendInterest (const Name &interest, const Closure &closure, const Selectors &selectors)
{
  _LOG_TRACE (">> sendInterest: " << interest);
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_ERROR ("<< sendInterest: not running or connected");
        return -1;
      }
  }

  CcnxCharbufPtr namePtr = interest.toCcnxCharbuf();
  ccn_charbuf *pname = namePtr->getBuf();
  ccn_closure *dataClosure = new ccn_closure;

  // Closure *myClosure = new ExecutorClosure(closure, m_executor);
  Closure *myClosure = closure.dup ();
  dataClosure->data = new tuple<Closure*, ExecutorPtr, Selectors> (myClosure, m_executor, selectors);

  dataClosure->p = &incomingData;

  CcnxCharbufPtr selectorsPtr = selectors.toCcnxCharbuf();
  ccn_charbuf *templ = NULL;
  if (selectorsPtr)
  {
    templ = selectorsPtr->getBuf();
  }

  UniqueRecLock lock(m_mutex);
  if (ccn_express_interest (m_handle, pname, dataClosure, templ) < 0)
  {
    _LOG_ERROR ("<< sendInterest: ccn_express_interest FAILED!!!");
  }

  return 0;
}

int CcnxWrapper::setInterestFilter (const Name &prefix, const InterestCallback &interestCallback, bool record/* = true*/)
{
  _LOG_TRACE (">> setInterestFilter");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
  {
    return -1;
  }

  CcnxCharbufPtr ptr = prefix.toCcnxCharbuf();
  ccn_charbuf *pname = ptr->getBuf();
  ccn_closure *interestClosure = new ccn_closure;

  // interestClosure->data = new ExecutorInterestClosure(interestCallback, m_executor);

  interestClosure->data = new tuple<CcnxWrapper::InterestCallback *, ExecutorPtr> (new InterestCallback (interestCallback), m_executor); // should be removed when closure is removed
  interestClosure->p = &incomingInterest;

  int ret = ccn_set_interest_filter (m_handle, pname, interestClosure);
  if (ret < 0)
  {
    _LOG_ERROR ("<< setInterestFilter: ccn_set_interest_filter FAILED");
  }

  if (record)
    {
      m_registeredInterests.insert(pair<Name, InterestCallback>(prefix, interestCallback));
    }

  _LOG_TRACE ("<< setInterestFilter");

  return ret;
}

void
CcnxWrapper::clearInterestFilter (const Name &prefix, bool record/* = true*/)
{
  _LOG_TRACE (">> clearInterestFilter");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
    return;

  CcnxCharbufPtr ptr = prefix.toCcnxCharbuf();
  ccn_charbuf *pname = ptr->getBuf();

  int ret = ccn_set_interest_filter (m_handle, pname, 0);
  if (ret < 0)
  {
  }

  if (record)
    {
      m_registeredInterests.erase(prefix);
    }

  _LOG_TRACE ("<< clearInterestFilter");
}

Name
CcnxWrapper::getLocalPrefix ()
{
  struct ccn * tmp_handle = ccn_create ();
  int res = ccn_connect (tmp_handle, NULL);
  if (res < 0)
    {
      return Name();
    }

  string retval = "";

  struct ccn_charbuf *templ = ccn_charbuf_create();
  ccn_charbuf_append_tt(templ, CCN_DTAG_Interest, CCN_DTAG);
  ccn_charbuf_append_tt(templ, CCN_DTAG_Name, CCN_DTAG);
  ccn_charbuf_append_closer(templ); /* </Name> */
  // XXX - use pubid if possible
  ccn_charbuf_append_tt(templ, CCN_DTAG_MaxSuffixComponents, CCN_DTAG);
  ccnb_append_number(templ, 1);
  ccn_charbuf_append_closer(templ); /* </MaxSuffixComponents> */
  ccnb_tagged_putf(templ, CCN_DTAG_Scope, "%d", 2);
  ccn_charbuf_append_closer(templ); /* </Interest> */

  struct ccn_charbuf *name = ccn_charbuf_create ();
  res = ccn_name_from_uri (name, "/local/ndn/prefix");
  if (res < 0) {
  }
  else
    {
      struct ccn_fetch *fetch = ccn_fetch_new (tmp_handle);

      struct ccn_fetch_stream *stream = ccn_fetch_open (fetch, name, "/local/ndn/prefix",
                                                        NULL, 4, CCN_V_HIGHEST, 0);
      if (stream == NULL) {
      }
      else
        {
          ostringstream os;

          int counter = 0;
          char buf[256];
          while (true) {
            res = ccn_fetch_read (stream, buf, sizeof(buf));

            if (res == 0) {
              break;
            }

            if (res > 0) {
              os << string(buf, res);
            } else if (res == CCN_FETCH_READ_NONE) {
              if (counter < 2)
                {
                  ccn_run(tmp_handle, 1000);
                  counter ++;
                }
              else
                {
                  break;
                }
            } else if (res == CCN_FETCH_READ_END) {
              break;
            } else if (res == CCN_FETCH_READ_TIMEOUT) {
              break;
            } else {
              break;
            }
          }
          retval = os.str ();
          stream = ccn_fetch_close(stream);
        }
      fetch = ccn_fetch_destroy(fetch);
    }

  ccn_charbuf_destroy (&name);

  ccn_disconnect (tmp_handle);
  ccn_destroy (&tmp_handle);

  boost::algorithm::trim(retval);
  return Name(retval);
}

bool
CcnxWrapper::verify(PcoPtr &pco, double maxWait)
{
  return m_verifier->verify(pco, maxWait);
}

// This is needed just for get function implementation
struct GetState
{
  GetState (double maxWait)
  {
    double intPart, fraction;
    fraction = modf (std::abs(maxWait), &intPart);

    m_maxWait = date_time::second_clock<boost::posix_time::ptime>::universal_time()
      + boost::posix_time::seconds (intPart)
      + boost::posix_time::microseconds (fraction * 1000000);
  }

  PcoPtr
  WaitForResult ()
  {
    //_LOG_TRACE("GetState::WaitForResult start");
    boost::unique_lock<boost::mutex> lock (m_mutex);
    m_cond.timed_wait (lock, m_maxWait);
    //_LOG_TRACE("GetState::WaitForResult finish");

    return m_retval;
  }

  void
  DataCallback (Name name, PcoPtr pco)
  {
    //_LOG_TRACE("GetState::DataCallback, Name [" << name << "]");
    boost::unique_lock<boost::mutex> lock (m_mutex);
    m_retval = pco;
    m_cond.notify_one ();
  }

  void
  TimeoutCallback (Name name)
  {
    boost::unique_lock<boost::mutex> lock (m_mutex);
    m_cond.notify_one ();
  }

private:
  boost::posix_time::ptime m_maxWait;

  boost::mutex m_mutex;
  boost::condition_variable    m_cond;

  PcoPtr  m_retval;
};


PcoPtr
CcnxWrapper::get(const Name &interest, const Selectors &selectors, double maxWait/* = 4.0*/)
{
  _LOG_TRACE (">> get: " << interest);
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_ERROR ("<< get: not running or connected");
        return PcoPtr ();
      }
  }

  GetState state (maxWait);
  this->sendInterest (interest, Closure (boost::bind (&GetState::DataCallback, &state, _1, _2),
                                         boost::bind (&GetState::TimeoutCallback, &state, _1)),
                      selectors);
  return state.WaitForResult ();
}

}
