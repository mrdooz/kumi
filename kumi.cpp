#include "stdafx.h"
#include "file_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"
#include "path_utils.hpp"
#include "kumi.hpp"
#include "websocket_server.hpp"

#pragma comment(lib, "Ws2_32.lib")

static char g_module_name[MAX_PATH];
static bool g_started_as_log_server;

static bool start_second_process(const char *cmd_args) {
  PROCESS_INFORMATION process_information;
  ZeroMemory(&process_information, sizeof(process_information));
  STARTUPINFOA startup_info;
  ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  char filename[MAX_PATH], cmdline[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
  if (cmd_args[0] != '\0') 
    sprintf(cmdline, "%s %s", filename, cmd_args);
  else
    strcpy(cmdline, filename);
  BOOL res = CreateProcessA(filename, cmdline, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, 
    &startup_info, &process_information);
  CloseHandle(process_information.hProcess);
  CloseHandle(process_information.hThread);
  return !!res;
}

static int count_instances(const char *name) {
  int cnt = 0;
  PROCESSENTRY32 entry;
  entry.dwSize = sizeof(PROCESSENTRY32);
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  if (Process32First(snapshot, &entry)) {
    while (Process32Next(snapshot, &entry)) {
      if (!_stricmp(entry.szExeFile, name))
        ++cnt;
    }
  }
  CloseHandle(snapshot);
  return cnt;
}

static int run_app(HINSTANCE instance) {

  if (!APP.init(instance))
    return 1;

  int res = APP.run(NULL);

  App::close();

  return res;
}

static bool global_init() {
  GetModuleFileNameA(NULL, g_module_name, MAX_PATH);
  return true;
}

static bool global_close() {
  return true;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
#ifdef _DEBUG
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif

  if (!global_init())
    return 1;

  int res = run_app(instance);

  global_close();
  return res;
}
