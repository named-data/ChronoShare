#include "object-db-file.h"
#include <assert.h>

void
writeBytes(ostream &out, const Bytes &bytes)
{
  int size = bytes.size();
  writeInt(out, size);
  out.write(head(bytes), size);
}

void
readBytes(istream &in, Bytes &bytes)
{
  int size;
  readInt(in, size);
  bytes.reserve(size);
  in.read(head(bytes), size);
}

ObjectDBFile::ObjectDBFile(const string &filename)
                : m_size(0)
                , m_cap(0)
                , m_index(0)
                , m_initialized(false)
                , m_filename(filename)
                // This ensures file with filename exists (assuming having write permission)
                // This is needed as file_lock only works with existing file
                , m_ostream(m_filename, ios_base::binary | ios_base::app)
                , m_istream(m_filename, ios_base::binary | ios_base::in)
                , m_filelock(m_filename)
{
  int magic;
  ReadLock(m_filelock);
  readInt(m_istream, magic);
  if (magic == MAGIC_NUM)
  {
    m_initialized = true;
    readInt(m_istream, m_cap);
    readInt(m_istream, m_size);
    m_istream.seekg( (3 + m_cap) * sizeof(int), ios::beg);
  }
}

ObjectDBFile::~ObjectDBFile()
{
  m_istream.close();
  m_ostream.close();
}

void
ObjectDBFile::init(int capacity)
{
  WriteLock(m_filelock);
  if (m_initialized)
  {
    throwException("Trying to init already initialized ObjectDBFile object" + m_filename);
  }

  m_cap = capacity;
  m_size = 0;

  int magic = MAGIC_NUM;
  writeInt(m_ostream, magic);
  writeInt(m_ostream, m_cap);
  writeInt(m_ostream, m_size);
  m_initialized = true;

  int count = size;
  int offset = 0;
  while (count-- > 0)
  {
    writeInt(m_ostream, offset);
  }

  // prepare read pos
  m_istream.seekg(m_ostream.tellp(), ios::beg);

  // DEBUG
  assert(m_ostream.tellp() == ((3 + m_cap) * sizeof(int)));

}

// Append is not super efficient as it needs to seek and update the pos for the
// Content object. However, in our app, it is the case the these objects are wrote
// once and read multiple times, so it's not a big problem.
void
ObjectDBFile::append(const Bytes &co)
{
  WriteLock(m_filelock);
  checkInit("Trying to append to un-initialized ObjectDBFile: " + m_filename);

  if (m_size >= m_cap)
  {
    throwException("Exceed Maximum capacity: " + boost::lexical_cast<string>(m_cap));
  }

  // pos for this CO
  int coPos = m_ostream.tellp();
  // index field for this CO
  int indexPos = (3 + m_size) * sizeof(int);

  m_size++;

  // Update size (is it necessary?) We'll do it for now anyway
  m_ostream.seekp( 2 * sizeof(int), ios::beg);
  writeInt(m_ostream, m_size);

  // Write the pos for the CO
  m_ostream.seekp(indexPos, ios::beg);
  writeInt(m_ostream, coPos);

  // write the content object
  m_ostream.seekp(coPos, ios::beg);
  writeBytes(m_ostream, co);

  // By the end, the write pos is at the end of the file
}

Bytes
ObjectDBFile::next()
{
}

void
ObjectDBFile::read(vector<Bytes> &vco, int n)
{
}

int
ObjectDBFile::size() const
{
  return m_size;
}

void
ObjectDBFile::updateSzie()
{
  int pos = m_istream.tellg();
  m_istream.seekg(2 * sizeof(int), ios::beg);
  readInt(m_istream, m_size);
  // recover the original pos
  m_istream.seekg(pos, ios::beg);
}

int
ObjectDBFile::fSize()
{
  ReadLock(m_filelock);
  updateSize();
  return m_size;
}

int
ObjectDBFile::index()
{
  ReadLock(m_filelock);
  return m_index;
}

bool
ObjectDBFile::seek(int index)
{
  ReadLock(m_filelock);
  updateSize();
  if (m_size <= index)
  {
    return false;
  }
  m_index = index;
  m_istream.seekg( (3 + m_index) * sizeof(int), ios::beg);
  int pos;
  readInt(m_istream, pos);
  m_istream.seekg(pos, ios::beg);
  return true;
}

void
rewind()
{
  ReadLock(m_filelock);
  m_index = 0;
  // point to the start of the CO fields
  m_istream.seekg( (3 + m_cap) * sizeof(int), ios::beg);
}

void
ObjectDBFile::checkInit(const string &msg)
{
  if (!m_initialized)
  {
    throwException(msg);
  }
}
