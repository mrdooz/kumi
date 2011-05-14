#include "stdafx.h"
#include "file_watcher.hpp"
#include "resource_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"

void file_changed(FileEvent event, const char *old_name, const char *new_name)
{
}

void file_changed2(const char *filename, const void *buf, size_t len)
{
}

void file_loaded(const char *filename, const void *buf, size_t len)
{
	string str((const char *)buf, len);

}

bool bool_true()
{
	return true;
}

bool bool_false()
{
	return false;
}

HRESULT hr_ok()
{
	return S_OK;
}

HRESULT hr_meh()
{
	return E_INVALIDARG;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
	if (!APP.init(instance))
		return 1;

	APP.run();

	APP.close();

	return 0;
}
#if 0
int main(int argc, char *argv[])
{


	const char *filename = "c:\\temp\\state1.txt";
	RESOURCE_WATCHER.add_file_watch(filename, true, file_changed2);

	RET_ERR_DX(hr_meh());

	//LOG(bool_true(), CHK_BOOL, LOG_ERROR_LN);
	//LOG(bool_false(), CHK_BOOL, LOG_ERROR_LN);

/*
	const char *filename = "c:\\temp\\state1.txt";
	ASYNC_FILE_LOADER.load_file(filename, file_loaded);


	HANDLE h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	size_t size = GetFileSize(h, NULL);
	char *buf = new char[size];
	DWORD res;
	ReadFile(h, buf, size, &res, NULL);
	CloseHandle(h);

	vector<Section> sections;
	extract_sections(buf, size, &sections);

	delete [] buf;

	FILE_WATCHER.add_file_watch("c:\\temp\\state1.txt", file_changed);
	while (!GetAsyncKeyState(VK_ESCAPE)) {
		Sleep(1000);
	}


	FILE_WATCHER.remove_watch(file_changed);
	FILE_WATCHER.close();
*/

	while (!GetAsyncKeyState(VK_ESCAPE)) {
		Sleep(1000);
	}

	return 0;
}
#endif