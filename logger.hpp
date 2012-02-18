#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <string>
#include <deque>
#include <map>
#include <set>
#include <windows.h>

/**
 * The Logger is designed to be a stand alone, drop in logging component. The class itself
 * is a singleton, and logging is done via the LOG_xxx family of macros below
 */

// TODO: add some kind of global filter that we can toggle on/off (grep/regex)

#define LOG_MGR LogMgr::instance()

#define LOG_VERBOSE(fmt, ...) LOG_MGR.debug_output(false, false, __FILE__, __LINE__, LogMgr::Verbose, fmt, __VA_ARGS__ );
#define LOG_VERBOSE_LN(fmt, ...) LOG_MGR.debug_output(true, false, __FILE__, __LINE__, LogMgr::Verbose, fmt, __VA_ARGS__ );
#define LOG_INFO(fmt, ...) LOG_MGR.debug_output(false, false, __FILE__, __LINE__, LogMgr::Info, fmt, __VA_ARGS__ );
#define LOG_INFO_LN(fmt, ...) LOG_MGR.debug_output(true, false, __FILE__, __LINE__, LogMgr::Info, fmt, __VA_ARGS__ );
#define LOG_WARNING(fmt, ...) do { LOG_MGR.debug_output(false, false, __FILE__, __LINE__, LogMgr::Warning, fmt, __VA_ARGS__ ); } while(false)
#define LOG_WARNING_LN(fmt, ...) do { LOG_MGR.debug_output(true, false, __FILE__, __LINE__, LogMgr::Warning, fmt, __VA_ARGS__ ); } while(false)
#define LOG_WARNING_LN_ONESHOT(fmt, ...) LOG_MGR.debug_output(true, true, __FILE__, __LINE__, LogMgr::Warning, fmt, __VA_ARGS__ );
#define LOG_ERROR(fmt, ...) LOG_MGR.debug_output(false, false, __FILE__, __LINE__, LogMgr::Error, fmt, __VA_ARGS__ );
#define LOG_ERROR_LN(fmt, ...) LOG_MGR.debug_output(true, false, __FILE__, __LINE__, LogMgr::Error, fmt, __VA_ARGS__ );
#define LOG_ERROR_LN_ONESHOT(fmt, ...) LOG_MGR.debug_output(true, true, __FILE__, __LINE__, LogMgr::Error, fmt, __VA_ARGS__ );
#define LOG_FATAL(fmt, ...) LOG_MGR.debug_output(false, false, __FILE__, __LINE__, LogMgr::Fatal, fmt, __VA_ARGS__ );
#define LOG_FATAL_LN(fmt, ...) LOG_MGR.debug_output(true, false, __FILE__, __LINE__, LogMgr::Fatal, fmt, __VA_ARGS__ );

using std::set;
using std::map;
using std::string;

bool check_bool(bool value, const char *exp, string *out);
bool check_hr(HRESULT hr, const char *exp, string *out);

#define LOG_INNER(x, chk, sev, ok) decltype((x)) res__ = (x); string err; if (!(ok = chk(res__, #x, &err))) sev(err.c_str());
#define LOG(x, chk, sev) { bool ok; LOG_INNER(x, chk, sev, ok) }
#define RET(x, chk, sev) { bool ok; LOG_INNER(x, chk, sev, ok); if (!ok) return res; }

#define CHK_HR(x, exp, res) check_hr(x, exp, res)
#define CHK_BOOL(x, exp, res) check_bool(x, exp, res)
#define CHK_INT(x, exp, res) check_bool(!!x, exp, res)

// define some shortcuts
#define RET_ERR_HR(x) RET(x, CHK_HR, LOG_ERROR_LN);
#define RET_ERR_DX(x) RET(x, CHK_DX, LOG_ERROR_LN);
#define RET_ERR_BOOL(x) RET(x, CHK_BOOL, LOG_ERROR_LN);

//#define ERR(x) { bool ok; LOG_INNER(x, CHK_HR, LOG_ERROR_LN, ok); if (!ok) return ok; }

#define B_ERR_HR(x) { bool ok; LOG_INNER(x, CHK_HR, LOG_ERROR_LN, ok); if (!ok) return ok; }
#define B_ERR_BOOL(x) { bool ok; LOG_INNER(x, CHK_BOOL, LOG_ERROR_LN, ok); if (!ok) return ok; }
#define B_ERR_INT(x) { bool ok; LOG_INNER(x, CHK_INT, LOG_ERROR_LN, ok); if (!ok) return ok; }

#define B_WRN_HR(x) { bool ok; LOG_INNER(x, CHK_HR, LOG_WARNING_LN, ok); if (!ok) return ok; }
#define B_WRN_BOOL(x) { bool ok; LOG_INNER(x, CHK_BOOL, LOG_WARNING_LN, ok); if (!ok) return ok; }

#define H_ERR_HR(x) RET(x, CHK_HR, LOG_ERROR_LN);
#define H_ERR_BOOL(x) RET(x, CHK_BOOL, LOG_ERROR_LN);

#define LOG_ERR_HR(x) LOG(x, CHK_HR, LOG_ERROR_LN);
#define LOG_ERR_BOOL(x) LOG(x, CHK_BOOL, LOG_ERROR_LN);

#define LOG_WRN_HR(x) LOG(x, CHK_HR, LOG_WARNING_LN);
#define LOG_WRN_BOOL(x) LOG(x, CHK_BOOL, LOG_WARNING_LN);

#define LOG_INF_HR(x) LOG(x, CHK_HR, LOG_INFO_LN);
#define LOG_INF_BOOL(x) LOG(x, CHK_BOOL, LOG_INFO_LN);

class LogMgr 
{
public:
  enum OuputDevice
  {
    Debugger    = (1 << 0),
    File        = (1 << 1),
  };

  enum Severity 
  {
    Verbose     = (1 << 0),
    Info        = (1 << 1),
    Warning     = (1 << 2),
    Error       = (1 << 3),
    Fatal       = (1 << 4),
  };

  void debug_output(const bool newLine, const bool one_shot, const char *file, const int line, const Severity severity, const char* const format, ...  );

	LogMgr& print_file_and_line(const bool value);
  LogMgr& enable_output(OuputDevice output);
  LogMgr& disable_output(OuputDevice output);
  LogMgr& open_output_file(const TCHAR *filename);
  LogMgr& break_on_error(const bool setting);

  LogMgr& enable_severity(const OuputDevice output, const Severity severity);
  LogMgr& disable_severity(const OuputDevice output, const Severity severity);

  static LogMgr& instance();
  static void close();

	HANDLE handle() const { return _file; }

private:
  LogMgr();
  ~LogMgr();

  void	init_severity_map();

	HANDLE _file;
  int   _output_device;
  bool _break_on_error;
	bool _output_line_numbers;

  set<string> one_shot_set_;
  map<OuputDevice, map<Severity, bool> > severity_map_;
  static LogMgr* _instance;
};

#endif
