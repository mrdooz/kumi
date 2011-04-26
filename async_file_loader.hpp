#pragma once

class AsyncFileLoader
{
public:
	typedef FastDelegate3<const char * /*filename*/, const uint8_t * /*buf*/, size_t /*len*/> CbFileLoaded;

	AsyncFileLoader();

	static AsyncFileLoader &instance();
	bool load_file(const char *filename, const CbFileLoaded &cb);

private:

	static void CALLBACK load_file_apc(ULONG_PTR data);
	static void CALLBACK terminate_apc(ULONG_PTR data);
	static void CALLBACK on_completion(DWORD error_Code, DWORD bytes_transfered, OVERLAPPED *overlapped);

	struct LoadData {
		HANDLE h;
		CbFileLoaded cb;
		OVERLAPPED overlapped;
	};

	static UINT __stdcall threadProc(void *userdata);
	bool lazy_create_thread();

	bool _terminating;
	HANDLE _thread;
	static AsyncFileLoader *_instance;
};

#define ASYNC_FILE_LOADER AsyncFileLoader::instance()
