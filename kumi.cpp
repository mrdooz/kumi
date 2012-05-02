#include "stdafx.h"
#include "file_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"
#include "path_utils.hpp"
#include "log_server.hpp"
#include "kumi.hpp"
#include "websocket_server.hpp"

#pragma comment(lib, "Ws2_32.lib")

static string g_module_name;
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

#if WITH_CEF
  CefSettings settings;
  settings.multi_threaded_message_loop = false;
  CefInitialize(settings);
#endif
  if (!APP.init(instance))
    return 1;

  int res = APP.run(NULL);

  App::close();

  return res;
}

static bool global_init() {
  char filename[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
  g_module_name = filename;
  return true;
}

static bool global_close() {
#if WITH_ZMQ_LOGSERVER
  HWND h = g_started_as_log_server ? FindWindow(g_app_window_class, g_app_window_title) :
    FindWindow(g_log_window_class, g_log_window_title);
  PostMessage(h, WM_APP_CLOSE, 0, 0);
#endif
  return true;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{

  if (!global_init())
    return 1;

#if WITH_WEBSOCKETS
  WebSocketThread ws_thread;
  ws_thread.start();
#endif

  int res = 0;

#if WITH_ZMQ_LOGSERVER
  // hopefully this should save me from killing myself :)
  Path path(g_module_name);
  int process_count = count_instances(path.get_filename().c_str());
  if (process_count > 2)
    return 1;

  g_started_as_log_server = !strcmp(cmd_line, "--log-server");
  if (g_started_as_log_server) {
    start_second_process("");
    res = run_log_server(instance);
  } else {
    start_second_process("--log-server");
    res = run_app(instance);
  }
#else
  res = run_app(instance);
#endif

#if WITH_WEBSOCKETS
  ws_thread.stop();
#endif

  global_close();
  return res;
}
