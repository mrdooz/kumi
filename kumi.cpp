#include "stdafx.h"
#include "file_watcher.hpp"
#include "async_file_loader.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "app.hpp"


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
  return 0;
}

bool start_log_server() {

  char filename[MAX_PATH];
  GetModuleFileNameA(NULL, filename, MAX_PATH);
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
