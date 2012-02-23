#include "stdafx.h"
#include "file_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"

#pragma comment(lib, "Ws2_32.lib")

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

int run_log_server() {

  void *ctx = zmq_init(1);
  void *s = zmq_socket(ctx, ZMQ_REP);
  zmq_bind(socket, "ipc:///tmp/feeds/0");
  //while (true) {
    zmq_msg_t query;
    zmq_msg_init(&query);
    zmq_recv(socket, &query, 0);

    MessageBoxA(NULL, (const char *)zmq_msg_data(&query), NULL, 0);

  //}

  zmq_close(s);
  zmq_term(ctx);

  return 0;
}

bool start_log_server() {

  PROCESS_INFORMATION process_information;
  ZeroMemory(&process_information, sizeof(process_information));
  STARTUPINFOA startup_info;
  ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  char filename[MAX_PATH], cmdline[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
  sprintf(cmdline, "%s --log-server", filename);
  BOOL res = CreateProcessA(filename, cmdline, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, 
    &startup_info, &process_information);

  void *ctx = zmq_init(1);
  void *s = zmq_socket(ctx, ZMQ_REQ);
  zmq_bind(socket, "ipc:///tmp/feeds/0");

  zmq_msg_t q;
  zmq_msg_init_size(&q, 7);
  memcpy(zmq_msg_data(&q), "magnus", 7);
  zmq_send(s, &q, 0);

  zmq_close(s);
  zmq_term(ctx);


  return !!res;
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


int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
  if (!strcmp(cmd_line, "--log-server")) {
    return run_log_server();
  } else {
    return run_app(instance);
  }
}
