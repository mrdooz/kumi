#ifndef _FILE_UTILS_HPP_
#define _FILE_UTILS_HPP_

bool load_file(const char *filename, void **buf, size_t *size);
bool load_file(const char *filename, std::vector<uint8> *buf);
bool file_exists(const char *filename);
string replace_extension(const char *org, const char *new_ext);
string strip_extension(const char *str);
void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext);

#endif
