#include "stdafx.h"
#include "path_utils.hpp"


string Path::make_canonical(const string& str)
{
  // convert back slashes to forward

  string res;
  for (int i = 0, e = str.size(); i < e; ++i) {
    const char ch = str[i];
    if (ch == '\\') {
      res += '/';
    } else {
      res += ch;
    }
  }
  return res;
}


Path::Path(const string& str) 
  : _str(make_canonical(str)) 
  , _ext_ofs(-1)
  , _file_ofs(-1)
{
  for (int i = _str.size() - 1; i >= 0; --i) {
		char ch = _str[i];
    if (ch == '.') {
			_ext_ofs = i;
		} else if (ch == '/') {
			// last slash
			_file_ofs = i;
			break;
		}
  }
}

Path Path::replace_extension(const string& ext)
{
  if (_ext_ofs == -1) {
    return Path(_str + "." + ext);
  }

  return Path(string(_str, _ext_ofs) + "." + ext);
}

const string& Path::str() const
{
  return _str;
}

string Path::get_path() const
{
	string res;
	if (_file_ofs != -1)
		res = _str.substr(0, _file_ofs + 1);
	return res;
}

string Path::get_ext() const
{
	string res;
	if (_ext_ofs != -1)
		res.assign(&_str[_ext_ofs+1]);
	return res;
}

string Path::get_filename() const
{
	string res;
	if (_file_ofs != -1)
		res.assign(&_str[_file_ofs+1]);
	return res;
}

string Path::get_filename_without_ext() const
{
	string res;
	if (_file_ofs != -1) {
		// 0123456
		// /tjong.ext
		// ^     ^--- _ext_ofs
		// +--------- _file_ofs
		int end = _ext_ofs == -1 ? _str.size() : _ext_ofs;
		res = _str.substr(_file_ofs + 1, end - _file_ofs - 1);
	}
  return res;
}

string Path::get_full_path_name(const string& p)
{
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

string Path::get_path(const string& p)
{
	Path a(p);
	return a.get_path();
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
