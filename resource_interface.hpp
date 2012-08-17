#pragma once

// The resource manager is a file system abstraction, allowing
// easy use of dropbox etc while developing, and a zipfile or something
// for the final release

struct ResourceInterface {
  static bool create(const char *outputFilename);
  static bool close();
  static ResourceInterface &instance();

  typedef std::function<bool (const char *, void *)> cbFileChanged;

  virtual void add_path(const std::string &path) {}

  virtual bool file_exists(const char *filename) = 0;
  virtual __time64_t mdate(const char *filename) = 0;
  virtual bool load_partial(const char *filename, size_t ofs, size_t len, std::vector<char> *buf) = 0;
  virtual bool load_file(const char *filename, std::vector<char> *buf) = 0;
  virtual bool load_inplace(const char *filename, size_t ofs, size_t len, void *buf) = 0;

  virtual void add_file_watch(const char *filename, void *token, const cbFileChanged &cb, 
    bool initial_callback, bool *initial_result, int timeout);
  virtual void remove_file_watch(const cbFileChanged &cb) {}
  virtual std::string resolve_filename(const char *filename, bool fullPath) = 0;
};

#define RESOURCE_MANAGER ResourceInterface::instance()
