#include "stdafx.h"
#include "log_server.hpp"
#include "kumi.hpp"
extern "C" {
#include "TreeList.h"
}

using namespace std;

static const char *g_log_server_addr = "tcp://*:5555";
const TCHAR *g_log_window_class = _T("Log Server");
const TCHAR *g_log_window_title = _T("Log Server");

#pragma comment(lib, "comctl32.lib")

HWND g_list_view;
HWND g_main_window;
bool g_timer_active;

TREELIST_HANDLE g_treelist_handle;

vector<zmq_msg_t *> g_msg_queue;

static const int IDT_LOG_MSG_TIMER = 1;

#define CHECKED_ZMQ(x) \
  if ((x) < 0) {  \
  const char *str = zmq_strerror(zmq_errno());  \
  MessageBoxA(NULL, str, NULL, MB_ICONEXCLAMATION); \
  zmq_close(s); \
  zmq_term(ctx);  \
  return zmq_errno(); \
  }

unsigned int WINAPI ServerThread(void *ctx) {
  void *s = zmq_socket(ctx, ZMQ_PULL);
  CHECKED_ZMQ(zmq_bind(s, g_log_server_addr));

  while (true) {
    zmq_msg_t *r = new zmq_msg_t;
    CHECKED_ZMQ(zmq_msg_init(r));
    if (zmq_recv(s, r, 0) == -1) {
      delete r;
      if (zmq_errno() == ETERM)
        break;
    }

    // send the new message to the list box
    PostMessage(g_main_window, WM_LOG_NEW_MSG, (WPARAM)r, NULL);
  }
  zmq_close(s);
  return 0;
}

HWND CreateListview(HWND hParent, HINSTANCE hInst, DWORD dwStyle, const RECT& rc)
{
  dwStyle |= WS_CHILD | WS_VISIBLE;
  return 
    CreateWindowEx(0,   //extended styles
    WC_LISTVIEW,        //control 'class' name
    0,                  //control caption
    dwStyle,            //wnd style
    rc.left,            //position: left
    rc.top,             //position: top
    rc.right,           //width
    rc.bottom,          //height
    hParent,            //parent window handle
    NULL,
    hInst,              //instance
    0);                 //user defined info

}

LRESULT on_size(HWND hwnd, int w, int h, UINT flags) {
  MoveWindow(g_list_view, 0, 0, w, h, TRUE);
  SendMessage(g_list_view, LVM_ARRANGE, LVA_ALIGNTOP, 0);
  return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

  switch (msg) {

    case WM_CREATE: {
      g_treelist_handle = TreeListCreate(((CREATESTRUCT *)lparam)->hInstance, hwnd, NULL, 0, NULL);
      TreeListAddColumn(g_treelist_handle, "msg", TREELIST_LAST_COLUMN);
      return 0;
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_APP_CLOSE:
      DestroyWindow(hwnd);
      break;

    case WM_LOG_NEW_MSG: {
      g_msg_queue.push_back((zmq_msg_t *)wparam);
      if (!g_timer_active) {
        SetTimer(hwnd, IDT_LOG_MSG_TIMER, 100, NULL);
        g_timer_active = true;
      }
      return 0;
    }

    case WM_TIMER: {
      if (wparam == IDT_LOG_MSG_TIMER) {
        for (auto it = begin(g_msg_queue); it != end(g_msg_queue); ++it) {
          zmq_msg_t *p = (zmq_msg_t *)*it;
          TreeListNodeData data = {0};
          strncpy(data.Data, (const char *)zmq_msg_data(p), TREELIST_MAX_STRING);
          TreeListAddNode(g_treelist_handle, NULL, &data, 1);
          zmq_msg_close(p);
          delete p;
        }
        g_msg_queue.clear();
        g_timer_active = false;
        return 0;
      }
      break;
    }
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND create_window(HINSTANCE instance) {

  const TCHAR *cClassName = "LogServer";

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.lpszClassName = g_log_window_class;
  if (!RegisterClassEx(&wc))
    return NULL;

  int w = GetSystemMetrics(SM_CXSCREEN);
  int h = GetSystemMetrics(SM_CYSCREEN);

  HWND hwnd = CreateWindowEx(0,
                             g_log_window_class,
                             g_log_window_title,
                             WS_OVERLAPPEDWINDOW,
                             w/4,
                             h/4,
                             w/2,
                             h/2,
                             NULL,
                             NULL,
                             instance,
                             NULL);

  if (!hwnd)
    return NULL;

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  return hwnd;
}

void start_common_controls(DWORD flags)
{
  //load the common controls dll, specifying the type of control(s) required 
  INITCOMMONCONTROLSEX iccx;
  iccx.dwSize = sizeof(iccx);
  iccx.dwICC = flags;
  InitCommonControlsEx(&iccx);
}

int run_log_server(HINSTANCE instance) {

  g_main_window = create_window(instance);
  if (!g_main_window)
    return 1;

  start_common_controls(ICC_LISTVIEW_CLASSES);

  void *ctx = zmq_init(1);
  HANDLE thread_id = (HANDLE)_beginthreadex(NULL, 0, ServerThread, ctx, 0, NULL);

  MSG msg;
  while (GetMessage(&msg, 0, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  zmq_term(ctx);
  WaitForSingleObject(thread_id, INFINITE);
  CloseHandle(thread_id);
  return 0;
}
