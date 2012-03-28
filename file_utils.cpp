#include "stdafx.h"
#include "file_utils.hpp"
#include "utils.hpp"

using namespace std;

string replace_extension(const char *org, const char *new_ext) {
	const char *dot = strrchr(org, '.');
	if (!dot)
		return org;
	return string(org, dot - org + 1) + new_ext;
}

string strip_extension(const char *str) {
	const char *dot = strrchr(str, '.');
	if (!dot)
		return str;
	return string(str, dot - str);
}

void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext) {
	char drive_buf[_MAX_DRIVE];
	char dir_buf[_MAX_DIR];
	char fname_buf[_MAX_FNAME];
	char ext_buf[_MAX_EXT];
	_splitpath(path, drive_buf, dir_buf, fname_buf, ext_buf);
	if (drive) *drive = drive_buf;
	if (dir) *dir = dir_buf;
	if (fname) *fname = fname_buf;
	if (ext) *ext = ext_buf;
}

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
