#pragma once

typedef FastDelegate3<const char * /*filename*/, const void * /*buf*/, size_t /*len*/> CbFileLoaded;

class AsyncFileLoader
{
public:
	AsyncFileLoader();

	static AsyncFileLoader &instance();
	bool load_file(const char *filename, const CbFileLoaded &cb);
	void remove_callback(const CbFileLoaded &cb);
	void close();

private:

	bool lazy_create_thread();

	HANDLE _thread;
	static AsyncFileLoader *_instance;
};

#define ASYNC_FILE_LOADER AsyncFileLoader::instance()
