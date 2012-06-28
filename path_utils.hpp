#ifndef _PATH_UTILS_HPP_
#define _PATH_UTILS_HPP_

using std::string;

class Path
{
public:
  Path();
  Path(const string& str);
  void init(const string &str);
  Path replace_extension(const string& ext);
  const string& str() const;
  string get_path() const;
  string get_ext() const;
  string get_filename() const;
  string get_filename_without_ext() const;

  static string make_canonical(const string &str);
  static string get_full_path_name(const string& p);
  static string replace_extension(const string& path, const string& ext);
  static string get_path(const string& p);

  static bool is_absolute(const std::string &path);

private:
  string _str;
  int _filename_ofs;    // points to the last '/'
  int _ext_ofs;         // points to the '.'
};

std::string replace_extension(const std::string &org, const std::string &new_ext);
std::string strip_extension(const std::string &str);
std::string get_extension(const std::string &str);
void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext);

#endif