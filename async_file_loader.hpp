#pragma once

typedef std::function<void (const char *, const void *, size_t)> CbFileLoaded;

class AsyncFileLoader
{
public:

	static AsyncFileLoader &instance();
  static void close();

	bool load_file(const char *filename, const CbFileLoaded &cb);
	void cancel_load(const char *filename);
	void remove_callback(const CbFileLoaded &cb);

private:
  AsyncFileLoader();
  ~AsyncFileLoader();

	bool lazy_create_thread();

	HANDLE _thread;
	static AsyncFileLoader *_instance;
};

#define ASYNC_FILE_LOADER AsyncFileLoader::instance()
