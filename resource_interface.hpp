#pragma once

struct ResourceInterface {

  typedef std::function<void (const char *, void *)> cbFileChanged;

  virtual bool file_exists(const char *filename) = 0;
  virtual __time64_t mdate(const char *filename) = 0;
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, void **buf) = 0;
  virtual bool load_file(const char *filename, void **buf, size_t *len) = 0;
  virtual bool load_file(const char *filename, std::vector<uint8> *buf) = 0;

  virtual bool supports_file_watch() const = 0;
  virtual void add_file_watch(const char *filename, bool initial_callback, void *token, const cbFileChanged &cb) = 0;
  virtual void remove_file_watch(const cbFileChanged &cb) = 0;
  virtual string resolve_filename(const char *filename) = 0;
};