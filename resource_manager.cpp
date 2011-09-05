#include "stdafx.h"
#include "utils.hpp"
#include "file_utils.hpp"
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
	string res(path);
	for (size_t i = 0; i < res.size(); ++i) {
		if (res[i] == '\\')
			res[i] = '/';
	}

	if (!res.empty() && res.back() != '/')
		res.push_back('/');

	return res;
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
	_paths.push_back(normalize_path(path));
}

bool ResouceManager::load_file(const char *filename, void **buf, size_t *len) {

	const string &full_path = resolve_filename(filename);
	if (full_path.empty())
		return false;

	return ::load_file(full_path.c_str(), buf, len);
}

bool ResouceManager::load_partial(const char *filename, size_t ofs, size_t len, void **buf) {
	const string &full_path = resolve_filename(filename);

	ScopedHandle h(CreateFileA(full_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
	if (h.handle() == INVALID_HANDLE_VALUE)
		return false;

	DWORD size = GetFileSize(h, NULL);
	if (ofs + len > size)
		return false;

	// if no buffer is supplied, allocate a new one
	const bool mem_supplied = *buf != NULL;
	if (!mem_supplied)
		*buf = new uint8_t[len];

	DWORD res;
	if (SetFilePointer(h, ofs, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		if (!mem_supplied)
			delete [] exch_null(*buf);
		return false;
	}

	if (!ReadFile(h, *buf, len, &res, NULL)) {
		DWORD err = GetLastError();
		if (!mem_supplied)
			delete [] exch_null(*buf);
		return false;
	}

	return true;

}

bool ResouceManager::file_exists(const char *filename) {
	return !resolve_filename(filename).empty();
}

__time64_t ResouceManager::mdate(const char *filename) {
	struct _stat s;
	_stat(filename, &s);
	return s.st_mtime;

}

bool ResouceManager::is_watchable() const {
#ifdef _DISTRIBUTION_
	return false;
#else
	return true;
#endif
}

void ResouceManager::copy_on_load(bool enable, const char *dest) {
	_copy_on_load = enable;
	if (enable)
		_copy_dest;
}

string ResouceManager::resolve_filename(const char *filename) {
#if _DEBUG
	// warn about duplicates
	int count = 0;
	string res;
	for (size_t i = 0; i < _paths.size(); ++i) {
		string cand(_paths[i] + filename);
		if (::file_exists(cand.c_str())) {
			count++;
			if (res.empty())
				res = cand;
		}
	}
	if (count > 1)
		LOG_WARNING_LN("Multiple paths resolved for file: %s", filename);
	return res;
#else
	for (size_t i = 0; i < _paths.size(); ++i) {
		string cand(_paths[i] + filename);
		if (file_exists(cand.c_str()))
			return normalize_path(cand.c_str());
	}
	return string();
#endif
}
