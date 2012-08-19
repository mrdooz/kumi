#include "stdafx.h"
#if !WITH_UNPACKED_RESOUCES
#include "utils.hpp"
#include "file_utils.hpp"
#include "packed_resource_manager.hpp"
#include "file_watcher.hpp"
#include "threading.hpp"
#include "logger.hpp"
#include "path_utils.hpp"
#include "graphics.hpp"
#include "lz4/lz4.h"

using namespace std::tr1::placeholders;
using namespace std;

static PackedResourceManager *g_instance;
static const int cMaxFileBufferSize = 16 * 1024 * 1024;

static uint32 FnvHash(uint32 d, const char *str) {
  if (d == 0)
    d = 0x01000193;

  while (true) {
    char c = *str++;
    if (!c)
      return d;
    d = ((d * 0x01000193) ^ c) & 0xffffffff;
  }
}

struct PackedHeader {
  int headerSize;
  int numFiles;
};

PackedResourceManager &PackedResourceManager::instance() {
  KASSERT(g_instance);
  return *g_instance;
}

bool PackedResourceManager::create(const char *outputFilename) {
  KASSERT(!g_instance);
  g_instance = new PackedResourceManager(outputFilename);
  return true;
}

bool PackedResourceManager::close() {
  delete exch_null(g_instance);
  return true;
}

PackedResourceManager::PackedResourceManager(const char *resourceFile) 
  : _resourceFile(resourceFile)
{
  FILE *f = fopen(resourceFile, "rb");
  LOG_ERROR_COND_LN(f, "Unable to open resource file: %s", resourceFile);
  DEFER([&]{ fclose(f); });

  PackedHeader header;
  LOG_ERROR_COND_LN(fread(&header, sizeof(header), 1, f) == 1, "Error reading packed header");

  // read the perfect hash tables
  _intermediateHash.resize(header.numFiles);
  _finalHash.resize(header.numFiles);

  LOG_ERROR_COND_LN(fread(&_intermediateHash[0], sizeof(int), header.numFiles, f) == header.numFiles, "Error reading hash");
  LOG_ERROR_COND_LN(fread(&_finalHash[0], sizeof(int), header.numFiles, f) == header.numFiles, "Error reading hash");

  _fileInfo.resize(header.numFiles);
  LOG_ERROR_COND_LN(fread(&_fileInfo[0], sizeof(PackedFileInfo), header.numFiles, f) == header.numFiles, "Error reading packed file info");

  int pos = ftell(f);
  fseek(f, 0, SEEK_END);
  int endPos = ftell(f);
  int dataSize = endPos - pos;
  fseek(f, pos, SEEK_SET);
  _fileBuffer.resize(dataSize);
  LOG_ERROR_COND_LN(fread(&_fileBuffer[0], 1, dataSize, f) == dataSize, "Error reading packed file data");
}

PackedResourceManager::~PackedResourceManager() {
}

int PackedResourceManager::hashLookup(const char *key) {
  int d = _intermediateHash[FnvHash(0, key) % _intermediateHash.size()];
  return d < 0 ? _finalHash[-d-1] : _finalHash[FnvHash(d, key) % _finalHash.size()];
}

bool PackedResourceManager::loadPackedFile(const char *filename, std::vector<char> *buf) {
  PackedFileInfo *p = &_fileInfo[hashLookup(filename)];
  buf->resize(p->finalSize);
  int res = LZ4_uncompress(&_fileBuffer[p->offset], buf->data(), p->finalSize);
  return res == p->compressedSize;
}

bool PackedResourceManager::loadPackedInplace(const char *filename, size_t ofs, size_t len, void *buf) {
  vector<char> tmp;
  if (!loadPackedFile(filename, &tmp))
    return false;

  memcpy(buf, tmp.data() + ofs, len);
  return true;
}

bool PackedResourceManager::load_file(const char *filename, std::vector<char> *buf) {
  return loadPackedFile(filename, buf);
}


bool PackedResourceManager::load_partial(const char *filename, size_t ofs, size_t len, std::vector<char> *buf) {
  // this is kinda cheesy..
  vector<char> tmp;
  if (!loadPackedFile(filename, &tmp))
    return false;
  buf->resize(len);
  copy(begin(tmp) + ofs, begin(tmp) + ofs + len, begin(*buf));
  return true;
}

bool PackedResourceManager::load_inplace(const char *filename, size_t ofs, size_t len, void *buf) {
  return loadPackedInplace(filename, ofs, len, buf);
}

GraphicsObjectHandle PackedResourceManager::load_texture(const char *filename, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info) {
  vector<char> tmp;
  loadPackedFile(filename, &tmp);
  return GRAPHICS.load_texture_from_memory(tmp.data(), tmp.size(), friendly_name, srgb, info);
}

#endif
