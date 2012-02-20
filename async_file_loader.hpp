#pragma once

typedef std::function<void (const char *, const void *, size_t)> CbFileLoaded;

class AsyncFileLoader
{
public:
	AsyncFileLoader();

	static AsyncFileLoader &instance();
	bool load_file(const char *filename, const CbFileLoaded &cb);
	void cancel_load(const char *filename);
	void remove_callback(const CbFileLoaded &cb);
	void close();

private:

	bool lazy_create_thread();

	HANDLE _thread;
	static AsyncFileLoader *_instance;
};

#define ASYNC_FILE_LOADER AsyncFileLoader::instance()
