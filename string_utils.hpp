#ifndef _STRING_UTILS_HPP_
#define _STRING_UTILS_HPP_

using std::string;
using std::wstring;

string to_string(char const * const format, ... );
string trim(const string &str);
bool wide_char_to_utf8(LPCOLESTR unicode, size_t len, string *str);
ustring ansi_to_host(const char *str);

bool begins_with(const char *str, const char *sub_str);

#endif
