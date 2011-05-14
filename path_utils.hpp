#ifndef _PATH_UTILS_HPP_
#define _PATH_UTILS_HPP_

using std::string;

class Path
{
public:
  Path(const string& str);
  Path replace_extension(const string& ext);
  const string& str() const;
  string get_path() const;
  string get_ext() const;
  string get_filename() const;
  string get_filename_without_ext() const;

	static string make_canonical(const string& str);
	static string get_full_path_name(const string& p);
  static string replace_extension(const string& path, const string& ext);
	static string get_path(const string& p);

private:
  string _str;
  int _file_ofs;		// points to the last '/'
  int _ext_ofs;			// points to the '.'
};

#endif