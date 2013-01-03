#ifndef CCNX_COMMON_H
#define CCNX_COMMON_H

extern "C" {
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/uri.h>
#include <ccn/bloom.h>
#include <ccn/signing.h>
}
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/exception/all.hpp>
#include <boost/function.hpp>
#include <string>
#include <sstream>
#include <map>
#include <utility>

using namespace std;
namespace Ccnx {
typedef vector<unsigned char> Bytes;
typedef vector<string>Comps;

// --- Bytes operations start ---
void
readRaw(Bytes &bytes, const unsigned char *src, size_t len);

const unsigned char *
head(const Bytes &bytes);

// --- Bytes operations end ---

// Exceptions
typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;

} // Ccnx
#endif // CCNX_COMMON_H
