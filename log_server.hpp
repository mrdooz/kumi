#pragma once

struct LogMessageHeader {
  enum Type {
    EnterScope,
    LeaveScope,
    LogMessage,
  };

  enum Severity {
    Unknown,
    Verbose,
    Info,
    Warning,
    Error,
  };

  uint16 type;
  uint16 severity;
  uint32 id;
#pragma warning(suppress: 4200)
  const char data[0];
};

class LogServer {
public:

private:
};

int run_log_server(HINSTANCE instance);
