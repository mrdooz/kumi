#include "stdafx.h"
#include "logger.hpp"
#include "utils.hpp"
#include "path_utils.hpp"
#include "string_utils.hpp"

#pragma comment(lib, "dxerr.lib")

static const char *g_client_addr = "tcp://127.0.0.1:5555";

bool check_bool(bool value, const char *exp, string *out) {
  *out = exp;
  return value;
}

bool check_hr(HRESULT hr, const char *exp, string *out) {
  if (SUCCEEDED(hr))
    return true;

  TCHAR *buf;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  FormatMessage(flags,  NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buf, 0, NULL);
  *out = to_string("[%s] : %s", exp, buf);
  LocalFree(buf); 
  return false;
}

Logger* Logger::_instance = NULL;

Logger::Logger() 
  : _file(INVALID_HANDLE_VALUE)
  , _output_device(Debugger | File)
  , _break_on_error(false)
  , _output_line_numbers(true)
  , _next_context_id(0)
{
  init_severity_map();
}

Logger::~Logger() {
  severity_map_.clear();

  if (_file != INVALID_HANDLE_VALUE)
    CloseHandle(_file);

}

void Logger::debug_output(bool new_line, bool one_shot, const char *file, int line, Severity severity, const char* const format, ...)
{
  va_list arg;
  va_start(arg, format);

  const int len = _vscprintf(format, arg) + 1 + (new_line ? 1 : 0);

  char* buf = (char*)_alloca(len);
  vsprintf_s(buf, len, format, arg);

  if (new_line) {
    buf[len-2] = '\n';
    buf[len-1] = 0;
  }

  if (one_shot) {
    if (one_shot_set_.end() != one_shot_set_.find(buf))
      return;
    one_shot_set_.insert(buf);
  }

  string str = (!file || !_output_line_numbers) ? buf : to_string("%s(%d): %s", file, line, buf);

  if ((_output_device & Logger::Debugger) && severity_map_[Logger::Debugger][severity])
    OutputDebugStringA(str.c_str());

  if (_output_device & File) {
    if (_file == INVALID_HANDLE_VALUE) {
      // no output file has been specified, so we use the current module as name
      char module[MAX_PATH+1];
      module[MAX_PATH] = 0;
      GetModuleFileNameA(NULL, module, MAX_PATH);
      open_output_file(ansi_to_host(Path::replace_extension(module, "log").c_str()).c_str());
    }
  }

  if (_file != INVALID_HANDLE_VALUE && (_output_device & Logger::File) && severity_map_[Logger::File][severity]) {
    DWORD bytes_written;
    WriteFile(_file, str.c_str(), str.size(), &bytes_written, NULL);
    if (severity >= Error)
      FlushFileBuffers(_file);
  }

  va_end(arg);

  if (_break_on_error && severity >= Logger::Error) 
    __asm int 3;
}

Logger& Logger::instance() {
  if (!_instance)
    _instance = new Logger();
  return *_instance;
}

void Logger::close() {
  delete exch_null(_instance);
}

Logger& Logger::enable_output(OuputDevice output) {
  _output_device |= output;
  return *this;
}

Logger& Logger::disable_output(OuputDevice output) {
  _output_device &= ~output;
  return *this;
}

Logger& Logger::open_output_file(const TCHAR *filename) {
  // close open file
  if (_file != INVALID_HANDLE_VALUE)
    CloseHandle(_file);

  SECURITY_ATTRIBUTES attr;
  ZeroMemory(&attr, sizeof(attr));
  attr.nLength = sizeof(attr);
  attr.bInheritHandle = TRUE;

  _file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, &attr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (_file == INVALID_HANDLE_VALUE)
    return *this;

  // move the end of log file
  SetFilePointer(_file, 0, 0, FILE_END);

  // write header
  time_t rawTime;
  tm timeInfo;
  time(&rawTime);
  if (0 != localtime_s(&timeInfo, &rawTime)) {
    // error
    return *this;
  }

  char buf[80];
  strftime(buf, sizeof(buf), "%H:%M:%S (%Y-%m-%d)", &timeInfo);

  char header[512];
  const int len = sprintf(header, "*****************************************************\n****\n**** Started at: %s\n****\n*****************************************************\n", buf);

  DWORD bytes_written = len;
  WriteFile(_file, header, len, &bytes_written, NULL);
  FlushFileBuffers(_file);

  return *this;
}

void Logger::init_severity_map() {
  severity_map_[Debugger][Verbose]  = false;
  severity_map_[Debugger][Info]     = true;
  severity_map_[Debugger][Warning]  = true;
  severity_map_[Debugger][Error]    = true;

  severity_map_[File][Verbose]  = true;
  severity_map_[File][Info]     = true;
  severity_map_[File][Warning]  = true;
  severity_map_[File][Error]    = true;
}

Logger& Logger::enable_severity(const OuputDevice output, const Severity severity) {
  severity_map_[output][severity] = true;
  return *this;
}

Logger& Logger::disable_severity(const OuputDevice output, const Severity severity) {
  severity_map_[output][severity] = false;
  return *this;
}

Logger& Logger::break_on_error(const bool setting) {
  _break_on_error = setting;
  return *this;
}

Logger& Logger::print_file_and_line(const bool value) {
  _output_line_numbers = value;
  return *this;
}

int Logger::get_next_context_id() {
  SCOPED_CS(_cs_context_id);
  return ++_next_context_id;
}

void Logger::enter_context(int context_id, const char *msg) {
}

void Logger::leave_context(int context_id) {
}

ScopedContext::ScopedContext(int id, bool is_spam, const char *fmt, ...) 
  : _id(id)
  , _is_spam(is_spam) {
  va_list arg;
  va_start(arg, fmt);

  const int len = _vscprintf(fmt, arg) + 1 + 1;

  char* buf = (char*)_alloca(len);
  vsprintf_s(buf, len, fmt, arg);
  buf[len-2] = '\n';
  buf[len-1] = 0;

  LOGGER.enter_context(id, buf);
  va_end(arg);
}

ScopedContext::~ScopedContext() {
  LOGGER.leave_context(_id);
}
