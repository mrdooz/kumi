#ifndef _FILE_UTILS_HPP_
#define _FILE_UTILS_HPP_

bool load_file(const char *filename, void **buf, size_t *size);
bool file_exists(const char *filename);

#endif
