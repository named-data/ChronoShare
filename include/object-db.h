#ifndef OBJECT_DB_H
#define OBJECT_DB_H

#include <boost/exception/all.hpp>
#include <vector>

using namespace std;

struct ObjectDBException : virtual boost::exception, virtual exception { };
typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;

void throwException(const string &msg) { boost::throw_exception(ObjectDBException() << error_info_str(msg)); }

typedef unsigned char Byte;
typedef vector<Byte> Bytes;

// OK. This name is a bit miss-leading, but this ObjectDB is really some storage
// that stores the Ccnx ContentObjects of a file. So unlike a normal database,
// this DB is per file.

// The assumption is, the ContentObjects will be write to ObjectDB sequentially
// This guarantees that when read, the Nth ContentObject read has the sequence number N as the last component of its name
class ObjectDB
{
public:
  virtual ~ObjectDB(){}

  // assume sequential
  virtual void
  append(const Bytes &co) = 0;

  // get next CO
  virtual Bytes
  next() = 0;

  // size in terms of number of COs
  virtual int
  size() = 0 const;

};

#endif
