#pragma once

#if !WITH_UNPACKED_RESOUCES
#include "graphics_object_handle.hpp"

class PackedResourceManager {
public:
  PackedResourceManager(const char *resourceFile);
  ~PackedResourceManager();

  static PackedResourceManager &instance();
  static bool create(const char *resourceFile);
  static bool close();

  bool load_file(const char *filename, std::vector<char> *buf);
  bool load_partial(const char *filename, size_t ofs, size_t len, std::vector<char> *buf);
  bool load_inplace(const char *filename, size_t ofs, size_t len, void *buf);
  GraphicsObjectHandle load_texture(const char *filename, const char *friendly_name, bool srgb, D3DX11_IMAGE_INFO *info);

private:

  bool loadPackedFile(const char *filename, std::vector<char> *buf);
  bool loadPackedInplace(const char *filename, size_t ofs, size_t len, void *buf);
  int hashLookup(const char *key);

  struct PackedFileInfo {
    int offset;
    int compressedSize;
    int finalSize;
  };

  std::vector<char> _fileBuffer;
  std::vector<int> _intermediateHash;
  std::vector<int> _finalHash;
  std::vector<PackedFileInfo> _fileInfo;

  static PackedResourceManager *_instance;
  std::string _resourceFile;
};

#endif