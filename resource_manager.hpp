#pragma once

// The resource manager is a file system abstraction, allowing
// easy use of dropbox etc while developing, and a zipfile or something
// for the final release

using std::vector;
using std::string;

class ResouceManager {
public:
	ResouceManager();

	static ResouceManager &instance();

	void copy_on_load(bool enable, const char *dest);

	void add_path(const char *path);

	bool load_file(const char *filename, void **buf, size_t *len);
	bool load_partial(const char *filename, size_t ofs, size_t len, void **buf);
	bool file_exists(const char *filename);
	__time64_t mdate(const char *filename);
	bool is_watchable() const;

private:
	string resolve_filename(const char *filename);
	static ResouceManager *_instance;
	bool _copy_on_load;
	vector<string> _paths;
};

#define RESOURCE_MANAGER ResouceManager::instance()
