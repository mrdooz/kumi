#include "stdafx.h"
#include "logger.hpp"
#include "utils.hpp"
#include "path_utils.hpp"
#include "string_utils.hpp"

#pragma comment(lib, "dxerr.lib")

bool check_bool(bool value, const char *exp, string *out)
{
	*out = exp;
	return value;
}

bool check_hr(HRESULT hr, const char *exp, string *out)
{
	if (SUCCEEDED(hr))
		return true;

	TCHAR *buf;
	const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	FormatMessage(flags,  NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buf, 0, NULL);
	*out = to_string("[%s] : %s", exp, buf);
	LocalFree(buf); 
	return false;
}

LogMgr* LogMgr::_instance = NULL;

LogMgr::LogMgr() 
  : _file(INVALID_HANDLE_VALUE)
  , _output_device(Debugger | File)
  , _break_on_error(false)
	, _output_line_numbers(true)
{
  init_severity_map();
}

LogMgr::~LogMgr() 
{
  severity_map_.clear();

	if (_file != INVALID_HANDLE_VALUE)
		CloseHandle(_file);
}

void LogMgr::debug_output(const bool new_line, const bool one_shot, const char *file, const int line, const Severity severity, const char* const format, ...  )
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

  std::string str = _output_line_numbers ? to_string("%s(%d): %s", file, line, buf) : buf;

  if ((_output_device & LogMgr::Debugger) && severity_map_[LogMgr::Debugger][severity])
    OutputDebugStringA(str.c_str());

  if (_output_device & File) {
    if (_file == INVALID_HANDLE_VALUE) {
      // no output file has been specified, so we use the current module as name
      char buf[MAX_PATH+1];
      buf[MAX_PATH] = 0;
      GetModuleFileNameA(NULL, buf, MAX_PATH);
      open_output_file(ansi_to_unicode(Path::replace_extension(buf, "log").c_str()).c_str());
    }
  }

  if (_file != INVALID_HANDLE_VALUE && (_output_device & LogMgr::File) && severity_map_[LogMgr::File][severity]) {
		DWORD bytes_written;
		WriteFile(_file, str.c_str(), str.size(), &bytes_written, NULL);
		if (severity >= Error)
			FlushFileBuffers(_file);
  }

  va_end(arg);

  if (_break_on_error && severity >= LogMgr::Error) 
    __asm int 3;
}

LogMgr& LogMgr::instance() 
{
  if (_instance == NULL) {
    _instance = new LogMgr();
    atexit(close);
  }
  return *_instance;
}

void LogMgr::close()
{
  SAFE_DELETE(_instance);
}

LogMgr& LogMgr::enable_output(OuputDevice output) 
{
  _output_device |= output;
  return *this;
}

LogMgr& LogMgr::disable_output(OuputDevice output) 
{
  _output_device &= ~output;
  return *this;
}

LogMgr& LogMgr::open_output_file(const TCHAR *filename) 
{
  // close open file
	if (_file != INVALID_HANDLE_VALUE)
		CloseHandle(_file);

	SECURITY_ATTRIBUTES attr;
	ZeroMemory(&attr, sizeof(attr));
	attr.nLength = sizeof(attr);
	attr.bInheritHandle = TRUE;

	if (INVALID_HANDLE_VALUE == (_file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, &attr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)))
		return *this;

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

void LogMgr::init_severity_map() 
{
  severity_map_[Debugger][Verbose]  = false;
  severity_map_[Debugger][Info]     = true;
  severity_map_[Debugger][Warning]  = true;
  severity_map_[Debugger][Error]    = true;
  severity_map_[Debugger][Fatal]    = true;

  severity_map_[File][Verbose]  = true;
  severity_map_[File][Info]     = true;
  severity_map_[File][Warning]  = true;
  severity_map_[File][Error]    = true;
  severity_map_[File][Fatal]    = true;
}

LogMgr& LogMgr::enable_severity(const OuputDevice output, const Severity severity) 
{
  severity_map_[output][severity] = true;
  return *this;
}

LogMgr& LogMgr::disable_severity(const OuputDevice output, const Severity severity) 
{
  severity_map_[output][severity] = false;
  return *this;
}

LogMgr& LogMgr::break_on_error(const bool setting) 
{
  _break_on_error = setting;
  return *this;
}

LogMgr& LogMgr::print_file_and_line(const bool value)
{
	_output_line_numbers = value;		
	return *this;
}
