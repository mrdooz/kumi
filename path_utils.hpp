#ifndef _PATH_UTILS_HPP_
#define _PATH_UTILS_HPP_

class Path
{
public:
  Path();
  Path(const std::string& str);
  void init(const std::string &str);
  Path replace_extension(const std::string& ext);
  const std::string& str() const;
  std::string get_path() const;
  std::string get_ext() const;
  std::string get_filename() const;
  std::string get_filename_without_ext() const;

  static std::string make_canonical(const std::string &str);
  static std::string get_full_path_name(const char *p);
  static std::string replace_extension(const std::string& path, const std::string& ext);
  static std::string get_path(const std::string& p);
  static std::string get_ext(const std::string &p);

  static bool is_absolute(const std::string &path);

  static std::string concat(const std::string &prefix, const std::string &suffix);

private:
  std::string _str;
  int _finalSlashPos;     // points to the last '/'
  int _extPos;            // points to the '.'
};

std::string replace_extension(const std::string &org, const std::string &new_ext);
std::string strip_extension(const std::string &str);
std::string stripTrailingSlash(const std::string &str);
std::string get_extension(const std::string &str);
void split_path(const char *path, std::string *drive, std::string *dir, std::string *fname, std::string *ext);

#endif