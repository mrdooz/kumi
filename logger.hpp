#pragma once
#include <string>
#include <deque>
#include <map>
#include <set>
#include <windows.h>
#include "utils.hpp"

class Logger
{
public:
  enum OuputDevice
  {
    Debugger    = (1 << 0),
    File        = (1 << 1),
  };

  enum Severity 
  {
    Unknown,
    Verbose,
    Info,
    Warning,
    Error,
  };

  void debug_output(bool newLine, bool one_shot, const char *file, int line, Severity severity, const char* const format, ...  );

  void enter_context(int context_id, const char *msg);
  void leave_context(int context_id);

  Logger& print_file_and_line(const bool value);
  Logger& enable_output(OuputDevice output);
  Logger& disable_output(OuputDevice output);
  Logger& open_output_file(const TCHAR *filename);
  Logger& break_on_error(bool setting);

  Logger& enable_severity(OuputDevice output, Severity severity);
  Logger& disable_severity(OuputDevice output, Severity severity);

  static Logger& instance();
  static void close();

  HANDLE handle() const { return _file; }

  int get_next_context_id();

private:
  Logger();
  ~Logger();

  void	init_severity_map();
  void send_log_message(int scope, Severity severity, const char *msg); // < 0 = leave scope, > 0 = enter scope, 0 = no scope

  HANDLE _file;
  int   _output_device;
  bool _break_on_error;
  bool _output_line_numbers;

  std::set<std::string> one_shot_set_;
  std::map<OuputDevice, std::map<Severity, bool> > severity_map_;
  static Logger* _instance;

  void *_context;
  void *_socket;
  bool _connected;
  CriticalSection _cs_context_id;
  int _next_context_id;

};


#define LOGGER Logger::instance()

#define LOG_VERBOSE(fmt, ...) LOGGER.debug_output(false, false, __FILE__, __LINE__, Logger::Verbose, fmt, __VA_ARGS__ );
#define LOG_VERBOSE_LN(fmt, ...) LOGGER.debug_output(true, false, __FILE__, __LINE__, Logger::Verbose, fmt, __VA_ARGS__ );
#define LOG_INFO(fmt, ...) LOGGER.debug_output(false, false, __FILE__, __LINE__, Logger::Info, fmt, __VA_ARGS__ );
#define LOG_INFO_LN(fmt, ...) LOGGER.debug_output(true, false, __FILE__, __LINE__, Logger::Info, fmt, __VA_ARGS__ );
#define LOG_WARNING(fmt, ...) do { LOGGER.debug_output(false, false, __FILE__, __LINE__, Logger::Warning, fmt, __VA_ARGS__ ); } while(false)
#define LOG_WARNING_LN(fmt, ...) do { LOGGER.debug_output(true, false, __FILE__, __LINE__, Logger::Warning, fmt, __VA_ARGS__ ); } while(false)
#define LOG_WARNING_LN_ONESHOT(fmt, ...) LOGGER.debug_output(true, true, __FILE__, __LINE__, Logger::Warning, fmt, __VA_ARGS__ );
#define LOG_ERROR(fmt, ...) LOGGER.debug_output(false, false, __FILE__, __LINE__, Logger::Error, fmt, __VA_ARGS__ );
#define LOG_ERROR_LN(fmt, ...) LOGGER.debug_output(true, false, __FILE__, __LINE__, Logger::Error, fmt, __VA_ARGS__ );
#define LOG_ERROR_LN_ONESHOT(fmt, ...) LOGGER.debug_output(true, true, __FILE__, __LINE__, Logger::Error, fmt, __VA_ARGS__ );
#define LOG_FATAL(fmt, ...) LOGGER.debug_output(false, false, __FILE__, __LINE__, Logger::Fatal, fmt, __VA_ARGS__ );
#define LOG_FATAL_LN(fmt, ...) LOGGER.debug_output(true, false, __FILE__, __LINE__, Logger::Fatal, fmt, __VA_ARGS__ );

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

class ScopedContext {
public:
  ScopedContext(int id, bool is_spam, const char *fmt, ...);
  ~ScopedContext();
private:
  int _id;
  bool _is_spam;
};

#define LOG_CONTEXT(fmt, ...) \
__declspec(thread) static int GEN_NAME(id, __LINE__) = __COUNTER__; \
  MAKE_SCOPED(ScopedContext)(GEN_NAME(id, __LINE__), false, fmt, __VA_ARGS__);

#define LOG_CONTEXT_SPAM(fmt, ...) \
  __declspec(thread) static int GEN_NAME(id, __LINE__) = __COUNTER__; \
  MAKE_SCOPED(ScopedContext)(GEN_NAME(id, __LINE__), true, fmt, __VA_ARGS__);
