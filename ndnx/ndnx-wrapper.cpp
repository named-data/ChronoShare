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

#include "ndnx-wrapper.h"
extern "C" {
#include <ndn/fetch.h>
}
#include <poll.h>
#include <boost/throw_exception.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <sstream>

#include "ndnx-verifier.h"
#include "logging.h"

INIT_LOGGER ("Ndnx.Wrapper");

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;
typedef boost::error_info<struct tag_errmsg, int> errmsg_info_int;

using namespace std;
using namespace boost;

namespace Ndnx {

// hack to enable fake signatures
// min length for signature field is 16, as defined in ndn_buf_decoder.c:728
const int DEFAULT_SIGNATURE_SIZE = 16;

// Although ndn_buf_decoder.c:745 defines minimum length 16, something else is checking and only 32-byte fake value is accepted by ndnd
const int PUBLISHER_KEY_SIZE = 32;

static int
ndn_encode_garbage_Signature(struct ndn_charbuf *buf)
{
    int res = 0;

    res |= ndn_charbuf_append_tt(buf, NDN_DTAG_Signature, NDN_DTAG);

    // Let's cheat more.  Default signing algorithm in ndnd is SHA256, so we just need add 32 bytes of garbage
    static char garbage [DEFAULT_SIGNATURE_SIZE];

    // digest and witness fields are optional, so use default ones

    res |= ndn_charbuf_append_tt(buf, NDN_DTAG_SignatureBits, NDN_DTAG);
    res |= ndn_charbuf_append_tt(buf, DEFAULT_SIGNATURE_SIZE, NDN_BLOB);
    res |= ndn_charbuf_append(buf, garbage, DEFAULT_SIGNATURE_SIZE);
    res |= ndn_charbuf_append_closer(buf);

    res |= ndn_charbuf_append_closer(buf);

    return(res == 0 ? 0 : -1);
}

static int
ndn_pack_unsigned_ContentObject(struct ndn_charbuf *buf,
                                const struct ndn_charbuf *Name,
                                const struct ndn_charbuf *SignedInfo,
                                const void *data,
                                size_t size)
{
    int res = 0;
    struct ndn_charbuf *content_header;
    size_t closer_start;

    content_header = ndn_charbuf_create();
    res |= ndn_charbuf_append_tt(content_header, NDN_DTAG_Content, NDN_DTAG);
    if (size != 0)
        res |= ndn_charbuf_append_tt(content_header, size, NDN_BLOB);
    closer_start = content_header->length;
    res |= ndn_charbuf_append_closer(content_header);
    if (res < 0)
        return(-1);

    res |= ndn_charbuf_append_tt(buf, NDN_DTAG_ContentObject, NDN_DTAG);

    res |= ndn_encode_garbage_Signature(buf);

    res |= ndn_charbuf_append_charbuf(buf, Name);
    res |= ndn_charbuf_append_charbuf(buf, SignedInfo);
    res |= ndnb_append_tagged_blob(buf, NDN_DTAG_Content, data, size);
    res |= ndn_charbuf_append_closer(buf);

    ndn_charbuf_destroy(&content_header);
    return(res == 0 ? 0 : -1);
}

NdnxWrapper::NdnxWrapper()
  : m_handle (0)
  , m_running (true)
  , m_connected (false)
  , m_executor (new Executor(1))
  , m_verifier(new Verifier(this))
{
  start ();
}

void
NdnxWrapper::connectNdnd()
{
  if (m_handle != 0) {
    ndn_disconnect (m_handle);
    //ndn_destroy (&m_handle);
  }
  else
    {
      m_handle = ndn_create ();
    }

  UniqueRecLock lock(m_mutex);
  if (ndn_connect(m_handle, NULL) < 0)
  {
    BOOST_THROW_EXCEPTION (NdnxOperationException() << errmsg_info_str("connection to ndnd failed"));
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

NdnxWrapper::~NdnxWrapper()
{
  shutdown ();
  if (m_verifier != 0)
  {
    delete m_verifier;
    m_verifier = 0;
}
}

void
NdnxWrapper::start () // called automatically in constructor
{
  connectNdnd();
  m_thread = thread (&NdnxWrapper::ndnLoop, this);
  m_executor->start();
}

void
NdnxWrapper::shutdown () // called in destructor, but can called manually
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

      ndn_disconnect (m_handle);
      //ndn_destroy (&m_handle);
      m_connected = false;
    }
}

void
NdnxWrapper::ndnLoop ()
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
            res = ndn_run (m_handle, 0);
          }

          if (!m_running) break;

          if (res < 0) {
            _LOG_ERROR ("ndn_run returned negative status: " << res);

            BOOST_THROW_EXCEPTION (NdnxOperationException()
                                   << errmsg_info_str("ndn_run returned error"));
          }


          pollfd pfds[1];
          {
            UniqueRecLock lock(m_mutex);

            pfds[0].fd = ndn_get_connection_fd (m_handle);
            pfds[0].events = POLLIN;
            if (ndn_output_is_pending (m_handle))
              pfds[0].events |= POLLOUT;
          }

          int ret = poll (pfds, 1, 1);
          if (ret < 0)
            {
              BOOST_THROW_EXCEPTION (NdnxOperationException() << errmsg_info_str("ndnd socket failed (probably ndnd got stopped)"));
            }
        }
        catch (NdnxOperationException &e)
        {
          m_connected = false;
          // probably ndnd has been stopped
          // try reconnect with sleep
          int interval = 1;
          int maxInterval = 32;
          while (m_running)
          {
            try
            {
              this_thread::sleep (boost::get_system_time () +  boost::posix_time::seconds (interval) + boost::posix_time::milliseconds (rangeUniformRandom ()));

              connectNdnd ();
              _LOG_DEBUG("reconnect to ndnd succeeded");
              break;
            }
            catch (NdnxOperationException &e)
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
NdnxWrapper::createContentObject(const Name  &name, const void *buf, size_t len, int freshness, const Name &keyNameParam)
{
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_TRACE ("<< not running or connected");
        return Bytes ();
      }
  }

  NdnxCharbufPtr ptr = name.toNdnxCharbuf();
  ndn_charbuf *pname = ptr->getBuf();
  ndn_charbuf *content = ndn_charbuf_create();

  struct ndn_signing_params sp = NDN_SIGNING_PARAMS_INIT;
  sp.freshness = freshness;

  Name keyName;

  if (keyNameParam.size() == 0)
  {
    // use default key name
    NdnxCharbufPtr defaultKeyNamePtr = boost::make_shared<NdnxCharbuf>();
    ndn_get_public_key_and_name(m_handle, &sp, NULL, NULL, defaultKeyNamePtr->getBuf());
    keyName = Name(*defaultKeyNamePtr);
  }
  else
  {
    keyName = keyNameParam;
  }

  if (sp.template_ndnb == NULL)
    {
    sp.template_ndnb = ndn_charbuf_create();
    ndn_charbuf_append_tt(sp.template_ndnb, NDN_DTAG_SignedInfo, NDN_DTAG);
    }
    // no idea what the following 3 lines do, but it was there
  else if (sp.template_ndnb->length > 0) {
      sp.template_ndnb->length--;
    }
  ndn_charbuf_append_tt(sp.template_ndnb, NDN_DTAG_KeyLocator, NDN_DTAG);
  ndn_charbuf_append_tt(sp.template_ndnb, NDN_DTAG_KeyName, NDN_DTAG);
  NdnxCharbufPtr keyPtr = keyName.toNdnxCharbuf();
  ndn_charbuf *keyBuf = keyPtr->getBuf();
  ndn_charbuf_append(sp.template_ndnb, keyBuf->buf, keyBuf->length);
  ndn_charbuf_append_closer(sp.template_ndnb); // </KeyName>
  ndn_charbuf_append_closer(sp.template_ndnb); // </KeyLocator>
  sp.sp_flags |= NDN_SP_TEMPL_KEY_LOCATOR;
  ndn_charbuf_append_closer(sp.template_ndnb); // </SignedInfo>

  if (ndn_sign_content(m_handle, content, pname, &sp, buf, len) != 0)
  {
    BOOST_THROW_EXCEPTION(NdnxOperationException() << errmsg_info_str("sign content failed"));
  }

  Bytes bytes;
  readRaw(bytes, content->buf, content->length);

  ndn_charbuf_destroy (&content);
  if (sp.template_ndnb != NULL)
  {
    ndn_charbuf_destroy (&sp.template_ndnb);
  }

  return bytes;
}

int
NdnxWrapper::putToNdnd (const Bytes &contentObject)
{
  _LOG_TRACE (">> putToNdnd");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
    {
      _LOG_TRACE ("<< not running or connected");
      return -1;
    }


  if (ndn_put(m_handle, head(contentObject), contentObject.size()) < 0)
  {
    _LOG_ERROR ("ndn_put failed");
    // BOOST_THROW_EXCEPTION(NdnxOperationException() << errmsg_info_str("ndnput failed"));
  }
  else
    {
      _LOG_DEBUG ("<< putToNdnd");
    }

  return 0;
}

int
NdnxWrapper::publishData (const Name &name, const unsigned char *buf, size_t len, int freshness, const Name &keyName)
{
  Bytes co = createContentObject(name, buf, len, freshness, keyName);
  return putToNdnd(co);
}

int
NdnxWrapper::publishUnsignedData(const Name &name, const unsigned char *buf, size_t len, int freshness)
{
  {
    UniqueRecLock lock(m_mutex);
    if (!m_running || !m_connected)
      {
        _LOG_TRACE ("<< not running or connected");
        return -1;
      }
  }

  NdnxCharbufPtr ptr = name.toNdnxCharbuf();
  ndn_charbuf *pname = ptr->getBuf();
  ndn_charbuf *content = ndn_charbuf_create();
  ndn_charbuf *signed_info = ndn_charbuf_create();

  static char fakeKey[PUBLISHER_KEY_SIZE];

  int res = ndn_signed_info_create(signed_info,
                                   fakeKey, PUBLISHER_KEY_SIZE,
                                   NULL,
                                   NDN_CONTENT_DATA,
                                   freshness,
                                   NULL,
                                   NULL  // ndnd is happy with absent key locator and key itself... ha ha
                                   );
  ndn_pack_unsigned_ContentObject(content, pname, signed_info, buf, len);

  Bytes bytes;
  readRaw(bytes, content->buf, content->length);

  ndn_charbuf_destroy (&content);
  ndn_charbuf_destroy (&signed_info);

  return putToNdnd (bytes);
}


static void
deleterInInterestTuple (tuple<NdnxWrapper::InterestCallback *, ExecutorPtr> *tuple)
{
  delete tuple->get<0> ();
  delete tuple;
}

static ndn_upcall_res
incomingInterest(ndn_closure *selfp,
                 ndn_upcall_kind kind,
                 ndn_upcall_info *info)
{
  NdnxWrapper::InterestCallback *f;
  ExecutorPtr executor;
  tuple<NdnxWrapper::InterestCallback *, ExecutorPtr> *realData = reinterpret_cast< tuple<NdnxWrapper::InterestCallback *, ExecutorPtr>* > (selfp->data);
  tie (f, executor) = *realData;

  switch (kind)
    {
    case NDN_UPCALL_FINAL: // effective in unit tests
      // delete closure;
      executor->execute (bind (deleterInInterestTuple, realData));

      delete selfp;
      _LOG_TRACE ("<< incomingInterest with NDN_UPCALL_FINAL");
      return NDN_UPCALL_RESULT_OK;

    case NDN_UPCALL_INTEREST:
      _LOG_TRACE (">> incomingInterest upcall: " << Name(info->interest_ndnb, info->interest_comps));
      break;

    default:
      _LOG_TRACE ("<< incomingInterest with NDN_UPCALL_RESULT_OK: " << Name(info->interest_ndnb, info->interest_comps));
      return NDN_UPCALL_RESULT_OK;
    }

  Name interest(info->interest_ndnb, info->interest_comps);
  Selectors selectors(info->pi);

  executor->execute (bind (*f, interest, selectors));
  // this will be run in executor
  // (*f) (interest);
  // closure->runInterestCallback(interest);

  return NDN_UPCALL_RESULT_OK;
}

static void
deleterInDataTuple (tuple<Closure *, ExecutorPtr, Selectors> *tuple)
{
  delete tuple->get<0> ();
  delete tuple;
}

static ndn_upcall_res
incomingData(ndn_closure *selfp,
             ndn_upcall_kind kind,
             ndn_upcall_info *info)
{
  // Closure *cp = static_cast<Closure *> (selfp->data);
  Closure *cp;
  ExecutorPtr executor;
  Selectors selectors;
  tuple<Closure *, ExecutorPtr, Selectors> *realData = reinterpret_cast< tuple<Closure*, ExecutorPtr, Selectors>* > (selfp->data);
  tie (cp, executor, selectors) = *realData;

  switch (kind)
    {
    case NDN_UPCALL_FINAL:  // effecitve in unit tests
      executor->execute (bind (deleterInDataTuple, realData));

      cp = NULL;
      delete selfp;
      _LOG_TRACE ("<< incomingData with NDN_UPCALL_FINAL");
      return NDN_UPCALL_RESULT_OK;

    case NDN_UPCALL_CONTENT:
      _LOG_TRACE (">> incomingData content upcall: " << Name (info->content_ndnb, info->content_comps));
      break;

    // this is the case where the intentionally unsigned packets coming (in Encapsulation case)
    case NDN_UPCALL_CONTENT_BAD:
      _LOG_TRACE (">> incomingData content bad upcall: " << Name (info->content_ndnb, info->content_comps));
      break;

    // always ask ndnd to try to fetch the key
    case NDN_UPCALL_CONTENT_UNVERIFIED:
      _LOG_TRACE (">> incomingData content unverified upcall: " << Name (info->content_ndnb, info->content_comps));
      break;

    case NDN_UPCALL_INTEREST_TIMED_OUT: {
      if (cp != NULL)
      {
        Name interest(info->interest_ndnb, info->interest_comps);
        _LOG_TRACE ("<< incomingData timeout: " << Name (info->interest_ndnb, info->interest_comps));
        executor->execute (bind (&Closure::runTimeoutCallback, cp, interest, *cp, selectors));
      }
      else
        {
          _LOG_TRACE ("<< incomingData timeout, but callback is not set...: " << Name (info->interest_ndnb, info->interest_comps));
        }
      return NDN_UPCALL_RESULT_OK;
    }

    default:
      _LOG_TRACE(">> unknown upcall type");
      return NDN_UPCALL_RESULT_OK;
    }

  PcoPtr pco = make_shared<ParsedContentObject> (info->content_ndnb, info->pco->offset[NDN_PCO_E]);

  // this will be run in executor
  executor->execute (bind (&Closure::runDataCallback, cp, pco->name (), pco));
  _LOG_TRACE (">> incomingData");

  return NDN_UPCALL_RESULT_OK;
}

int NdnxWrapper::sendInterest (const Name &interest, const Closure &closure, const Selectors &selectors)
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

  NdnxCharbufPtr namePtr = interest.toNdnxCharbuf();
  ndn_charbuf *pname = namePtr->getBuf();
  ndn_closure *dataClosure = new ndn_closure;

  // Closure *myClosure = new ExecutorClosure(closure, m_executor);
  Closure *myClosure = closure.dup ();
  dataClosure->data = new tuple<Closure*, ExecutorPtr, Selectors> (myClosure, m_executor, selectors);

  dataClosure->p = &incomingData;

  NdnxCharbufPtr selectorsPtr = selectors.toNdnxCharbuf();
  ndn_charbuf *templ = NULL;
  if (selectorsPtr)
  {
    templ = selectorsPtr->getBuf();
  }

  UniqueRecLock lock(m_mutex);
  if (ndn_express_interest (m_handle, pname, dataClosure, templ) < 0)
  {
    _LOG_ERROR ("<< sendInterest: ndn_express_interest FAILED!!!");
  }

  return 0;
}

int NdnxWrapper::setInterestFilter (const Name &prefix, const InterestCallback &interestCallback, bool record/* = true*/)
{
  _LOG_TRACE (">> setInterestFilter");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
  {
    return -1;
  }

  NdnxCharbufPtr ptr = prefix.toNdnxCharbuf();
  ndn_charbuf *pname = ptr->getBuf();
  ndn_closure *interestClosure = new ndn_closure;

  // interestClosure->data = new ExecutorInterestClosure(interestCallback, m_executor);

  interestClosure->data = new tuple<NdnxWrapper::InterestCallback *, ExecutorPtr> (new InterestCallback (interestCallback), m_executor); // should be removed when closure is removed
  interestClosure->p = &incomingInterest;

  int ret = ndn_set_interest_filter (m_handle, pname, interestClosure);
  if (ret < 0)
  {
    _LOG_ERROR ("<< setInterestFilter: ndn_set_interest_filter FAILED");
  }

  if (record)
    {
      m_registeredInterests.insert(pair<Name, InterestCallback>(prefix, interestCallback));
    }

  _LOG_TRACE ("<< setInterestFilter");

  return ret;
}

void
NdnxWrapper::clearInterestFilter (const Name &prefix, bool record/* = true*/)
{
  _LOG_TRACE (">> clearInterestFilter");
  UniqueRecLock lock(m_mutex);
  if (!m_running || !m_connected)
    return;

  NdnxCharbufPtr ptr = prefix.toNdnxCharbuf();
  ndn_charbuf *pname = ptr->getBuf();

  int ret = ndn_set_interest_filter (m_handle, pname, 0);
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
NdnxWrapper::getLocalPrefix ()
{
  struct ndn * tmp_handle = ndn_create ();
  int res = ndn_connect (tmp_handle, NULL);
  if (res < 0)
    {
      return Name();
    }

  string retval = "";

  struct ndn_charbuf *templ = ndn_charbuf_create();
  ndn_charbuf_append_tt(templ, NDN_DTAG_Interest, NDN_DTAG);
  ndn_charbuf_append_tt(templ, NDN_DTAG_Name, NDN_DTAG);
  ndn_charbuf_append_closer(templ); /* </Name> */
  // XXX - use pubid if possible
  ndn_charbuf_append_tt(templ, NDN_DTAG_MaxSuffixComponents, NDN_DTAG);
  ndnb_append_number(templ, 1);
  ndn_charbuf_append_closer(templ); /* </MaxSuffixComponents> */
  ndnb_tagged_putf(templ, NDN_DTAG_Scope, "%d", 2);
  ndn_charbuf_append_closer(templ); /* </Interest> */

  struct ndn_charbuf *name = ndn_charbuf_create ();
  res = ndn_name_from_uri (name, "/local/ndn/prefix");
  if (res < 0) {
  }
  else
    {
      struct ndn_fetch *fetch = ndn_fetch_new (tmp_handle);

      struct ndn_fetch_stream *stream = ndn_fetch_open (fetch, name, "/local/ndn/prefix",
                                                        NULL, 4, NDN_V_HIGHEST, 0);
      if (stream == NULL) {
      }
      else
        {
          ostringstream os;

          int counter = 0;
          char buf[256];
          while (true) {
            res = ndn_fetch_read (stream, buf, sizeof(buf));

            if (res == 0) {
              break;
            }

            if (res > 0) {
              os << string(buf, res);
            } else if (res == NDN_FETCH_READ_NONE) {
              if (counter < 2)
                {
                  ndn_run(tmp_handle, 1000);
                  counter ++;
                }
              else
                {
                  break;
                }
            } else if (res == NDN_FETCH_READ_END) {
              break;
            } else if (res == NDN_FETCH_READ_TIMEOUT) {
              break;
            } else {
              break;
            }
          }
          retval = os.str ();
          stream = ndn_fetch_close(stream);
        }
      fetch = ndn_fetch_destroy(fetch);
    }

  ndn_charbuf_destroy (&name);

  ndn_disconnect (tmp_handle);
  ndn_destroy (&tmp_handle);

  boost::algorithm::trim(retval);
  return Name(retval);
}

bool
NdnxWrapper::verify(PcoPtr &pco, double maxWait)
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
NdnxWrapper::get(const Name &interest, const Selectors &selectors, double maxWait/* = 4.0*/)
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
