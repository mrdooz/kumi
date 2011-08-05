#pragma once

struct Io {
	virtual bool load_file(const char *filename, void **buf, size_t *len) = 0;
	virtual bool load_partial(const char *filename, size_t ofs, size_t len, void **buf) = 0;
	virtual bool file_exists(const char *filename) = 0;
	virtual bool find_first(const char *filemask, string *filename, void **token) = 0;
	virtual bool find_next(void *token, string *filename) = 0;
	virtual void find_close(void *token) = 0;
	virtual __time64_t mdate(const char *filename) = 0;
};

class DiskIo : public Io {
public:
	virtual bool load_file(const char *filename, void **buf, size_t *len);
	virtual bool load_partial(const char *filename, size_t ofs, size_t len, void **buf);
	virtual bool file_exists(const char *filename);
	virtual bool find_first(const char *filemask, string *filename, void **token);
	virtual bool find_next(void *token, string *filename);
	virtual void find_close(void *token);
	virtual __time64_t mdate(const char *filename);
private:
};
