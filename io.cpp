#include "stdafx.h"
#include "io.hpp"
#include "file_utils.hpp"

bool DiskIo::load_file(const char *filename, void **buf, size_t *len) {
	return ::load_file(filename, buf, len);
}

bool DiskIo::file_exists(const char *filename) {
	return ::file_exists(filename);
}

__time64_t DiskIo::mdate(const char *filename) {
	struct _stat s;
	_stat(filename, &s);
	return s.st_mtime;
}
