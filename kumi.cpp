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

  if (!APP.init(instance))
    return 1;

  int res = APP.run(NULL);

  App::close();

  global_close();
  return res;
}
