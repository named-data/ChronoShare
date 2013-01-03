#ifndef OBJECT_DB_FILE_H
#define OBJECT_DB_FILE_H

#include "object-db.h"
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <deque>
#include <boost/thread/locks.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#define _OVERRIDE
#ifdef __GNUC__
#if __GNUC_MAJOR >= 4 && __GNUC_MINOR__ >= 7
  #undef _OVERRIDE
  #define _OVERRIDE override
#endif // __GNUC__ version
#endif // __GNUC__

using namespace std;

// This is a file based ObjectDB implementation
// The assumption is, the Content Objects will be stored sequentially

// To provide random access, we will have a table of "address" for each
// ContentObject at the beginning of the file.
// This also requires another assumption, that is the number of COs must
// be know a priori. This requirement is reasonable for our dropbox-like
// System, as the file we publish is static file and we can easily know
// the number of COs before we store them into ObjectDB.

/* How file looks like:
 * |MAGIC_NUM|capacity|size|pos for each CO ...|1st CO|2nd CO| ... |
 */

class ObjectDBFile
{
public:
  typedef boost::interprocess::file_lock Filelock;
  typedef boost::interprocess::scoped_lock<Filelock> WriteLock;
  typedef boost::interprocess::sharable_lock<Filelock> ReadLock;
  typedef boost::shared_mutex Mutex;
  typedef boost::shared_lock<Mutex> SLock;
  typedef boost::unique_lock<Mutex> ULock;

  ObjectDBFile(const string &filename);
  virtual ~ObjectDBFile();

  // reserve the "address" table for n COs; must reserve before
  // write anything (unless reserved quota has not be consumed yet)
  void
  init(int capacity);

  bool
  initialized() const { return m_initialized; }

  // assume sequential
  virtual void
  append(const Bytes &co) _OVERRIDE;

  // get next CO
  virtual Bytes
  next() _OVERRIDE;

  // size in terms of number of COs
  // This is the lazy form of size, i.e. it returns the size cached in this object
  // but that may not necessarily equal to the actual size kept in file
  // This is enough if the caller knows for sure that no other thread is changing the
  // file or the caller does not care about the new size.
  virtual int
  size() const _OVERRIDE;

  // this returns the actual size (also update the size cache in this object), but it is more costly, and requires file IO
  int
  fSize();

  // the index of the CO to be read
  int
  index();

  // set the pos to be the desired CO
  // return true if success
  bool
  seek(int index);

  // reset pos to be zero
  void
  rewind();

protected:
  // read or write lock should have been grabbed already before the call
  void
  checkInit(const string &msg);

  // read lock should have been grabbed already before the call
  void
  updateSize();

  // read lock should have been grabbed already before the call
  void
  fillDummyCache();

  #define MAGIC_NUM 0xAAAAAAAA

protected:
  string m_filename;
  ifstream m_istream;
  ofstream m_ostream;
  Filelock m_filelock;
  bool m_initialized;
  // capacity in terms of number of COs
  int m_cap;
  int m_size;
  // the index (or seq) of the CO to be read
  int m_index;

  // A dummy Cache that holds the next 10 (or all remaining if less than 10)
  // COs after a next() operation
  // If needed and time allows, we can have more complex cache
  #define CACHE_SIZE 10
  map<int, Bytes> m_dummyCache;
  Mutex m_cacheMutex;
};

void inline
writeInt(ostream &out, const int &x)
{
  out.write((const char *)&x, sizeof(int));
}

void inline
readInt(istream &in, int &x)
{
  in.read((char *)&x, sizeof(int));
}

// write size and then the actual bytes
// operator << overloading is not used to avoid confusion
void
writeBytes(ostream &out, const Bytes &bytes);

// read size and then the actual bytes
void
readBytes(istream &in, Bytes &bytes);

char *
head(const Bytes &bytes);

#endif
