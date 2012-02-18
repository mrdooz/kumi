#pragma once

struct ResourceInterface {

  typedef std::function<bool (const char *)> cbFileChanged;

  virtual bool file_exists(const char *filename) = 0;
  virtual __time64_t mdate(const char *filename) = 0;
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, void **buf) = 0;
  virtual bool load_file(const char *filename, void **buf, size_t *len) = 0;

  virtual bool supports_file_watch() const = 0;
  virtual void watch_file(const char *filename, const cbFileChanged &cb) = 0;
  virtual string resolve_filename(const char *filename) = 0;
};
