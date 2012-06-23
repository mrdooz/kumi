#include "stdafx.h"
#include "file_utils.hpp"
#include "utils.hpp"

using namespace std;

/*
bool load_file(const char *filename, std::vector<uint8> *buf) {
  ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  if (!h) return false;

  DWORD size = GetFileSize(h, NULL);
  buf->resize(size);
  DWORD res;
  if (!ReadFile(h, &(*buf)[0], size, &res, NULL)) 
    return false;
  return true;
}
*/
/*
bool load_file(const char *filename, void **buf, size_t *size)
{
	ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  if (!h) 
    return false;

	*size = GetFileSize(h, NULL);
	*buf = new uint8_t[*size];
	DWORD res;
	if (!ReadFile(h, *buf, *size, &res, NULL)) 
    return false;

	return true;
}
*/
bool file_exists(const char *filename)
{
	if (_access(filename, 0) != 0)
		return false;

	struct _stat status;
	if (_stat(filename, &status) != 0)
		return false;

	return !!(status.st_mode & _S_IFREG);
}
