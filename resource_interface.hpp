#pragma once

struct ResourceInterface {
	virtual bool file_exists(const char *filename) = 0;
	virtual __time64_t mdate(const char *filename) = 0;
	virtual bool load_file(const char *filename, void **buf, size_t *len) = 0;
};
	