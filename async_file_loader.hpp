#pragma once

class AsyncFileLoader
{
public:
	typedef FastDelegate3<const char * /*filename*/, const uint8_t * /*buf*/, size_t /*len*/> CbFileLoaded;

	AsyncFileLoader();

	static AsyncFileLoader &instance();
	bool load_file(const char *filename, const CbFileLoaded &cb);
	void close();

private:


	struct LoadData {
		HANDLE h;
		CbFileLoaded cb;
		OVERLAPPED overlapped;
	};

	bool lazy_create_thread();

	HANDLE _thread;
	static AsyncFileLoader *_instance;
};

#define ASYNC_FILE_LOADER AsyncFileLoader::instance()
