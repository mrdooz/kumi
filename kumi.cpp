#include "stdafx.h"
#include "file_watcher.hpp"
#include "resource_watcher.hpp"

void file_changed(FileWatcher::FileEvent event, const char *old_name, const char *new_name)
{
}


int main(int argc, char *argv[])
{
	const char *filename = "c:\\temp\\state1.txt";

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

	return 0;
}
