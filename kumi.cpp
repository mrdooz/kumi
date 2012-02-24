#include "stdafx.h"
#include "file_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"
#include "path_utils.hpp"
#include <TlHelp32.h>

#pragma comment(lib, "Ws2_32.lib")

static string g_module_name;
static const char *g_log_server_addr = "tcp://*:5556";


void file_changed(FileEvent event, const char *old_name, const char *new_name)
{
}

void file_changed2(const char *filename, const void *buf, size_t len)
{
}

void file_loaded(const char *filename, const void *buf, size_t len)
{
  string str((const char *)buf, len);

}

bool bool_true()
{
  return true;
}

bool bool_false()
{
  return false;
}

HRESULT hr_ok()
{
  return S_OK;
}

HRESULT hr_meh()
{
  return E_INVALIDARG;
}

bool start_process(const char *cmd_args);

int count_instances(const char *name) {
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


int run_log_server() {

  void *ctx = zmq_init(1);
  void *s = zmq_socket(ctx, ZMQ_REP);
  int res = zmq_bind(s, g_log_server_addr);
  if (res < 0) {
    const char *str = zmq_strerror(zmq_errno());
    MessageBoxA(NULL, str, NULL, MB_ICONEXCLAMATION);
    return zmq_errno();
  }

  start_process("");

  while (true) {
    zmq_msg_t r;
    zmq_msg_init(&r);
    zmq_recv(s, &r, 0);

    MessageBoxA(NULL, (const char *)zmq_msg_data(&r), NULL, 0);
    zmq_msg_close(&r);

  }

  zmq_close(s);
  zmq_term(ctx);

  return 0;
}

bool start_process(const char *cmd_args) {
  PROCESS_INFORMATION process_information;
  ZeroMemory(&process_information, sizeof(process_information));
  STARTUPINFOA startup_info;
  ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  char filename[MAX_PATH], cmdline[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
  if (strlen(cmd_args) > 0) 
    sprintf(cmdline, "%s %s", filename, cmd_args);
  else
    strcpy(cmdline, filename);
  return !!CreateProcessA(filename, cmdline, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, 
    &startup_info, &process_information);
}

bool start_log_server() {

  start_process("--log-server");

  void *ctx = zmq_init(1);
  void *s = zmq_socket(ctx, ZMQ_REQ);
  int z_res;
  if (zmq_bind(s, "tcp://127.0.0.1:5556") < 0) {
    int err = zmq_errno();
    int a = 10;
  }

  zmq_msg_t q;
  z_res = zmq_msg_init_size(&q, 7);
  strcpy((char *)zmq_msg_data(&q), "magnus");
  z_res = zmq_send(s, &q, 0);
  zmq_msg_close(&q);

  zmq_msg_t reply;
  z_res = zmq_recv(s, &reply, 0);

  zmq_close(s);
  zmq_term(ctx);

  return true;
}

int run_app(HINSTANCE instance) {

  start_log_server();

  CefSettings settings;
  settings.multi_threaded_message_loop = false;
  CefInitialize(settings);

  if (!APP.init(instance))
    return 1;

  int res = APP.run(NULL);

  APP.close();

  return res;
}

bool init() {
  char filename[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
  g_module_name = filename;
  return true;
}


int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
  if (!init())
    return 1;

  // hopefully this should save me from killing myself :)
  Path path(g_module_name);
  int process_count = count_instances(path.get_filename().c_str());
  if (process_count > 2)
    return 1;


  if (!strcmp(cmd_line, "--log-server")) {
    return run_log_server();
  } else {
    return run_app(instance);
  }
}
