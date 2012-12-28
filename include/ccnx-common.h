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

void
readRaw(Bytes &bytes, const unsigned char *src, size_t len);

const unsigned char *
head(const Bytes &bytes);

} // Ccnx
#endif // CCNX_COMMON_H
