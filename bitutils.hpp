#pragma once

inline uint32 set_bit(uint32 value, int bit_num) {
  return (value | 1 << bit_num);
}

inline bool is_bit_set(uint32 value, int bit_num) {
  return !!(value & (1 << bit_num));
}

inline uint32 clear_bit(uint32 value, int bit_num) {
  return value & ~(1 << bit_num);
}

inline int bits_required(uint32 input) {
  uint32 res = 1;
  while (input > (1U << res) - 1)
    ++res;
  return res;
}

inline int compress_value(int value, int bits) {
  assert(bits_required(value) + 1 <= bits);
  if (value < 0)
    value = set_bit(-value, bits - 1);
  return value;
}

inline int expand_value(uint32 value, int bits) {
  return (is_bit_set(value, bits - 1) ? -1 : 1) * clear_bit(value, bits - 1);
}

inline uint32 zigzag_encode(int n) {
  // https://developers.google.com/protocol-buffers/docs/encoding
  return (n << 1) ^ (n >> 31);
}

inline int zigzag_decode(int n) {
  return (n >> 1) ^ (-(n & 1));
}

class BitReader
{
public:
  BitReader(const uint8 *data, uint32 len_in_bits);
  uint32 read(uint32 count);
  uint32 read_varint(); 
  bool eof() const;
private:
  uint32 _length_in_bits;
  uint32 _bit_offset;
  uint32 _byte_offset;
  const uint8 *_data;
};

class BitWriter
{
public:
  BitWriter(uint32 buf_size = 1024);
  ~BitWriter();
  void write(uint32 value, uint32 num_bits);
  void write_varint(uint32 value);
  void get_stream(std::vector<uint8> *out, uint32 *bit_length);
  void get_stream(uint8 **out, uint32 *bit_length);
private:
  uint32 _bit_offset;
  uint32 _byte_offset;
  uint32 _buf_size;
  uint8 *_buf;
};
