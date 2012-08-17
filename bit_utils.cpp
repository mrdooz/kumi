#include "stdafx.h"
#include "bit_utils.hpp"

typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int32_t int32;
typedef int64_t int64;

using namespace std;


BitReader::BitReader(const uint8 *data, uint32 len_in_bits)
  : _length_in_bits(len_in_bits)
  , _bit_offset(0)
  , _byte_offset(0)
  , _data(data)
{
}

uint32 BitReader::read_varint() {
/*
  uint32 tmp = read(32);
  if (!(tmp & (1 << 7)))
    return tmp & 0x7f;

  if (!(tmp & (1 << 14)))
    return ((tmp & 0x7f) | ((tmp & 0x7f00) >> 1)) & 0x3fff;

  if (!(tmp & (1 << 21)))
    return ((tmp & 0x7f) | ((tmp & 0x7f00) >> 1) | ((tmp & 0x7f0000) >> 2)) & 0x3fffff;

  if (!(tmp & (1 << 28)))
    return ((tmp & 0x7f) | ((tmp & 0x7f00) >> 1) | ((tmp & 0x7f0000) >> 2) | ((tmp & 0x7f000000) >> 3)) & 0x3fffffff;

  uint32 t2 = read(8);

  return (t2 << 24) | ((tmp & 0x7f) | ((tmp & 0x7f00) >> 1) | ((tmp & 0x7f0000) >> 2) | ((tmp & 0x7f000000) >> 3));
*/
  uint32 res = 0;
  uint32 ofs = 0;
  while (true) {
    uint32 next = read(8);
    res |= ((next & 0x7f) << ofs);
    if (!(next & 0x80))
      return res;
    ofs += 7;
  }
}

uint32 BitReader::read(uint32 count) {

  static const uint32 read_mask[] = { 0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
    0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff, 0x1ffff, 0x3ffff, 0x7ffff,
    0xfffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};
/*
  uint64 res = *(uint64 *)&_data[_byte_offset] >> _bit_offset;

  // Calc new bit/byte offset
  uint32 bit_ofs = _bit_offset;
  _bit_offset = (bit_ofs + count) % 8;
  _byte_offset += (bit_ofs + count) / 8;

  return (uint32)(res & read_mask[count]);
*/

  // The 32 bit version is faster than the 64-bit version above..
  const uint32 *bb = (uint32 *)&_data[_byte_offset];
  // Read the coming 2 32 bit values
  const uint32 a = bb[0];
  const uint32 b = bb[1];

  // Calc new bit/byte offset
  const uint32 bit_ofs = _bit_offset;
  _bit_offset = (bit_ofs + count) % 8;
  _byte_offset += (bit_ofs + count) / 8;

  const uint32 num_lower_bits = 32 - bit_ofs;
  if (count > num_lower_bits) {
    const uint32 num_higher_bits = count - num_lower_bits;
    // Note, we don't need to mask out the lower bits if we know that we're reading
    // the full length
    return (a >> bit_ofs) | ((b & read_mask[num_higher_bits]) << num_lower_bits);
  } else {
    return (a >> bit_ofs) & read_mask[count];
  }

}

bool BitReader::eof() const {
  return _byte_offset * 8 + _bit_offset >= _length_in_bits;
}


BitWriter::BitWriter(uint32 buf_size)
  : _bit_offset(0)
  , _byte_offset(0)
  , _buf_size(buf_size)
  , _buf((uint8 *)malloc(_buf_size))
{
}

BitWriter::~BitWriter() {
  delete _buf;
}

void BitWriter::get_stream(uint8 **out, uint32 *bit_length) {
  *out = _buf;
  if (bit_length)
    *bit_length = _byte_offset * 8 + _bit_offset;
}

void BitWriter::get_stream(vector<uint8> *out, uint32 *bit_length) {
  size_t len = _byte_offset + (_bit_offset + 7) / 8;
  out->resize(len);
  memcpy(out->data(), _buf, len);
  if (bit_length)
    *bit_length = _byte_offset * 8 + _bit_offset;
}

void BitWriter::write_varint(uint32 value) {
  // based on https://developers.google.com/protocol-buffers/docs/encoding

  if (value < 1 << 7) {
    write(value, 8);

  } else if (value < 1 << 14) {
    const uint32 v0 = value & 0x7f;
    const uint32 v1 = (value >> 7) & 0x7f;
    write((v0 | 0x80) | (v1 << 8), 16);

  } else if (value < 1 << 21) {
    const uint32 v0 = value & 0x7f;
    const uint32 v1 = (value >> 7) & 0x7f;
    const uint32 v2 = (value >> 14) & 0x7f;
    write((v0 | 0x80) | ((v1 | 0x80) << 8) | (v2 << 16), 24);

  } else if (value < 1 << 28) {
    const uint32 v0 = value & 0x7f;
    const uint32 v1 = (value >> 7) & 0x7f;
    const uint32 v2 = (value >> 14) & 0x7f;
    const uint32 v3 = (value >> 21) & 0x7f;
    write((v0 | 0x80) | ((v1 | 0x80) << 8) | ((v2 | 0x80) << 16) | (v3 << 24), 32);

  } else {
    const uint32 v0 = value & 0x7f;
    const uint32 v1 = (value >> 7) & 0x7f;
    const uint32 v2 = (value >> 14) & 0x7f;
    const uint32 v3 = (value >> 21) & 0x7f;
    const uint32 v4 = (value >> 28) & 0x7f;
    write((v0 | 0x80) | ((v1 | 0x80) << 8) | ((v2 | 0x80) << 16) | ((v3 | 0x80) << 24), 32);
    write(v4, 8);
  }
}

void BitWriter::write(uint32 value, uint32 count) {

  static const uint64 read_mask[] = { 0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff };
  // Check if we need to resize (we need at least 64 bits due
  // to the 64 bit write)
  if (_byte_offset + 8 >= _buf_size) {
    uint8 *new_buf = (uint8 *)malloc(2 * _buf_size);
    memcpy(new_buf, _buf, _buf_size);
    free(_buf);
    _buf = new_buf;
    _buf_size *= 2;
  }

  uint64 *buf = (uint64 *)&_buf[_byte_offset];
  const uint32 bit_ofs = _bit_offset;
  _byte_offset += (bit_ofs + count) / 8;
  _bit_offset = (bit_ofs + count) % 8;

  // Because we're working on LE, we left shift to get zeros in the
  // bits that are already used.
  const uint64 prefix = (uint64)value << bit_ofs;
  buf[0] = (buf[0] & read_mask[bit_ofs]) | prefix;
}

#if 0
#define FUL_ASSERT(x) if (!x) _asm { int 3}


int _tmain(int argc, _TCHAR* argv[])
{

  LARGE_INTEGER freq, start, end;
  QueryPerformanceFrequency(&freq);

  QueryPerformanceCounter(&start);

  {
    BitWriter writer;
    writer.write_varint(10);
    writer.write(10, 5);
    writer.write_varint(10);
    writer.write(10, 5);
    //writer.unbuffered_write(256, 10);
    //writer.unbuffered_write(30, 20);

    vector<uint8> s;
    uint32 len;
    writer.get_stream(&s, &len);

    BitReader reader(s.data(), len);
    uint32 x, a;
    x = reader.read_varint();
    a = reader.read(5);
    x = reader.read_varint();
    a = reader.read(5);
    //uint32 b = reader.read(10);
    //uint32 c = reader.read(20);
    int d = 10;

  }

/*
  BitWriter writer;
  writer.write_varint(1 << 24);
  vector<uint8> s;
  uint32 len;
  writer.get_stream(&s, &len);

  BitReader reader(s.data(), len);
  uint32 value = reader.read_varint();
  int a = 10;
  */
#if _DEBUG
  int outer = 1;
  int inner_inc = 1;
#else
  int outer = 500;
  int inner_inc = 1;
#endif
  for (int i = 0; i < outer; ++i) {
    BitWriter writer(1024);
    for (int i = 0; i < 100000; i += inner_inc) {
      writer.write_varint(i*100);
      writer.write(i, 10 + i/100);
    }

    uint32 len;
    uint8 *s;
    //vector<uint8> s;
    writer.get_stream(&s, &len);

    BitReader reader(s, len);
    for (int i = 0; i < 100000; i += inner_inc) {
      int a = reader.read_varint();
      int res = reader.read(10 + i / 100);
      FUL_ASSERT(res == i);
      FUL_ASSERT(a == i*100);
    }
  }
  QueryPerformanceCounter(&end);

  double elapsed = (end.QuadPart - start.QuadPart) / (double)freq.QuadPart;
  printf("elapsed: %.3fs\n", elapsed);

	return 0;
}

#endif