#include "stdafx.h"
#include "path_utils.hpp"

using std::string;

string Path::make_canonical(const string &str) {
  // convert back slashes to forward
  string res(str);
  boost::replace_all(res, "\\", "/");
  return res;
}

Path::Path()
  : _ext_ofs(string::npos)
  , _filename_ofs(string::npos) {
}

Path::Path(const string& str) {
  init(str);
}

void Path::init(const string &str) {
  _str = make_canonical(str);
  _ext_ofs = _str.rfind(".");
  _filename_ofs = str.rfind("/");
}


Path Path::replace_extension(const string& ext) {
  return _ext_ofs == string::npos ? Path(_str + "." + ext) : Path(string(_str, _ext_ofs) + "." + ext);
}

const string& Path::str() const {
  return _str;
}

string Path::get_path() const {
  return _filename_ofs == string::npos ? string() : _str.substr(0, _filename_ofs + 1);
}

string Path::get_ext() const {
  return _ext_ofs == string::npos ? string() : string(&_str[_ext_ofs+1]);
}

string Path::get_filename() const {
  return _filename_ofs == string::npos ? string() : string(&_str[_filename_ofs+1]);
}

string Path::get_filename_without_ext() const {
  string res;
  if (_filename_ofs != string::npos) {
    // 0123456
    // /tjong.ext
    // ^     ^--- _ext_ofs
    // +--------- _filename_ofs
    int end = _ext_ofs == string::npos ? _str.size() : _ext_ofs;
    res = _str.substr(_filename_ofs + 1, end - _filename_ofs - 1);
  }
  return res;
}

string Path::get_full_path_name(const string& p) {
  char buf[MAX_PATH];
  GetFullPathNameA(p.c_str(), MAX_PATH, buf, NULL);
  return buf;
}

string Path::replace_extension(const string& path, const string& ext)
{
  string res;
  if (!path.empty() && !ext.empty()) {
    const char *dot = path.c_str();
    while (*dot && *dot++ != '.')
      ;

    if (*dot)
      res = string(path.c_str(), dot - path.c_str()) + ext;
    else
      res = path + (dot[-1] == '.' ? "" : ".") + ext;
  }

  return res;
}

string Path::get_path(const string& p) {
  Path a(p);
  return a.get_path();
}

bool Path::is_absolute(const std::string &path) {
  size_t len = path.size();
  return 
    len >= 1 && path[0] == '/' || 
    len >= 3 && path[1] == ':' && path[2] == '/';
}

string replace_extension(const std::string &org, const std::string &new_ext) {
  return strip_extension(org) + "." + new_ext;
}

string strip_extension(const std::string &str) {
  int idx = str.rfind('.');
  if (idx == string::npos)
    return str;
  return string(str.c_str(), idx);
}

std::string get_extension(const std::string &str) {
  int idx = str.rfind('.');
  if (idx == string::npos)
    return "";
  return string(str.c_str() + idx + 1, str.size() - idx);
}

void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext) {
  char drive_buf[_MAX_DRIVE];
  char dir_buf[_MAX_DIR];
  char fname_buf[_MAX_FNAME];
  char ext_buf[_MAX_EXT];
  _splitpath(path, drive_buf, dir_buf, fname_buf, ext_buf);
  if (drive) *drive = drive_buf;
  if (dir) *dir = dir_buf;
  if (fname) *fname = fname_buf;
  if (ext) *ext = ext_buf;
}

std::string Path::concat(const std::string &prefix, const std::string &suffix) {
  string res;
  res.reserve(prefix.size() + 1 + suffix.size());
  res.append(prefix);
  if (!prefix.empty() && prefix.back() != '\\' && prefix.back() != '/')
    res += '/';
  res.append(suffix);
  return res;

}
