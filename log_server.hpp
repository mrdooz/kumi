#pragma once

struct LogMessageHeader {
  enum Type {
    EnterScope,
    LeaveScope,
    LogMessage,
  };

  enum Severity {
    Verbose,
    Info,
    Warning,
    Error,
    Fatal,
  };

  uint16 type;
  uint16 severity;
#pragma warning(suppress: 4200)
  const char data[0];
};

class LogServer {
public:

private:
};

int run_log_server(HINSTANCE instance);
