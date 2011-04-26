#include "stdafx.h"
#include "utils.hpp"

bool wide_char_to_utf8(LPCOLESTR unicode, size_t len, string *str)
{
	if (!unicode)
		return false;

	char *buf = (char *)_alloca(len*2) + 1;

	int res;
	if (!(res = WideCharToMultiByte(CP_UTF8, 0, unicode, len, buf, len * 2 + 1, NULL, NULL)))
		return false;

	buf[len] = '\0';

	*str = string(buf);
	return true;
}

bool load_file(const char *filename, uint8_t **buf, size_t *size)
{
	HANDLE h = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	*size = GetFileSize(h, NULL);
	*buf = new uint8_t[*size];
	DWORD res;
	if (!ReadFile(h, *buf, *size, &res, NULL))
		return false;

	if (!CloseHandle(h))
		return false;

	return true;
}

string trim(const string &str) 
{
	int leading = 0, trailing = 0;
	while (isspace((uint8_t)str[leading]))
		leading++;
	const int end = str.size() - 1;
	while (isspace((uint8_t)str[end - trailing]))
		trailing++;

	return leading || trailing ? str.substr(leading, str.size() - (leading + trailing)) : str;
}


CriticalSection::CriticalSection() 
{
	InitializeCriticalSection(&_cs);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&_cs);
}

void CriticalSection::enter()
{
	EnterCriticalSection(&_cs);
}

void CriticalSection::leave()
{
	LeaveCriticalSection(&_cs);
}


ScopedCs::ScopedCs(CriticalSection &cs) 
	: _cs(cs)
{
	_cs.enter();
}

ScopedCs::~ScopedCs()
{
	_cs.leave();
}
