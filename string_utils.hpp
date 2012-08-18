#ifndef _STRING_UTILS_HPP_
#define _STRING_UTILS_HPP_

std::string to_string(char const * const format, ... );
std::string trim(const std::string &str);
bool wide_char_to_utf8(LPCOLESTR unicode, size_t len, std::string *str);
std::string wide_char_to_utf8(const std::wstring &str);
ustring ansi_to_host(const char *str);
std::wstring utf8_to_wide(const char *str);

bool begins_with(const char *str, const char *sub_str);
bool begins_with(const std::string &str, const std::string &sub_str);

#if WITH_WEBSOCKETS
std::string base64encode(const uint8_t *buf, int buf_size);
#endif

#endif
