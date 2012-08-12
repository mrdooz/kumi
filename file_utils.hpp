#ifndef _FILE_UTILS_HPP_
#define _FILE_UTILS_HPP_

template <typename T>
bool load_file(const char *filename, std::vector<T> *buf) {
  ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  if (!h) return false;

  DWORD size = GetFileSize(h, NULL);
  if (size > 0) {
    buf->resize(size);
    DWORD res;
    if (!ReadFile(h, &(*buf)[0], size, &res, NULL)) 
      return false;
  }
  return true;
}

bool file_exists(const char *filename);

bool save_file(const char *filename, const void *buf, int len);

#endif
