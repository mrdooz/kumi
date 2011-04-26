#pragma once

using std::string;
bool wide_char_to_utf8(LPCOLESTR unicode, size_t len, string *str);
bool load_file(const char *filename, uint8_t **buf, size_t *size);
string trim(const string &str);

class CriticalSection {
public:
	CriticalSection();
	~CriticalSection();
	void enter();
	void leave();
private:
	CRITICAL_SECTION _cs;
};

class ScopedCs {
public:
	ScopedCs(CriticalSection &cs);
	~ScopedCs();
private:
	CriticalSection &_cs;
};

#define SCOPED_CS(cs) ScopedCs lock(cs);
