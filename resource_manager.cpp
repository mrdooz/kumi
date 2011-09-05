#include "stdafx.h"
#include "resource_manager.hpp"

ResouceManager *ResouceManager::_instance = nullptr;

static bool file_exists(const char *filename)
{
	if (_access(filename, 0) != 0)
		return false;

	struct _stat status;
	if (_stat(filename, &status) != 0)
		return false;

	return !!(status.st_mode & _S_IFREG);
}

static string normalize_path(const char *path) {

}


ResouceManager::ResouceManager() 
	: _copy_on_load(false)
{
	_paths.push_back("./");
}

ResouceManager &ResouceManager::instance() {
	if (!_instance)
		_instance = new ResouceManager;
	return *_instance;
}

void ResouceManager::add_path(const char *path) {
	_paths.push_back(path);
}

bool ResouceManager::load_file(const char *filename, void **buf, size_t *len) {

	ScopedHandle h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
	if (h.handle() == INVALID_HANDLE_VALUE)
		return false;

	*size = GetFileSize(h, NULL);
	*buf = new uint8_t[*size];
	DWORD res;
	if (!ReadFile(h, *buf, *size, &res, NULL))
		return false;

	return true;
}

bool ResouceManager::load_partial(const char *filename, size_t ofs, size_t len, void **buf) {

}

bool ResouceManager::file_exists(const char *filename) {

}

__time64_t ResouceManager::mdate(const char *filename) {

}

bool ResouceManager::is_watchable() const {

}

void ResouceManager::copy_on_load(bool enable, const char *dest) {

}
