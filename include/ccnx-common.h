#ifndef CCNX_COMMON_H
#define CCNX_COMMON_H

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

// --- Name operation start ---
void
split(const string &name, Comps &comps);
// --- Name operation end ---

} // Ccnx
#endif // CCNX_COMMON_H
