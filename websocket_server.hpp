#pragma once
#if WITH_WEBSOCKETS

#include "threading.hpp"

class WebSocketServer;

class WebSocketThread : public threading::BlockingThread {
  friend WebSocketServer;
public:

protected:
  WebSocketThread();
  void stop();
  void send_msg(SOCKET receiver, const char *msg, int len);
  virtual void on_idle() {}
  UINT blocking_run(void *data);
  int num_clients_connected();
private:
  static UINT __stdcall client_thread(void *param);

  CriticalSection _thread_cs;
  struct ClientData {
    friend bool operator==(const ClientData &lhs, const ClientData &rhs) { return lhs.thread == rhs.thread; }
    SOCKET socket;
    WebSocketThread *self;
    HANDLE close_event;
    HANDLE thread;
  };

  std::vector<ClientData *> _client_threads;
};

class WebSocketServer {
public:
  static WebSocketServer& instance();
  static bool create();
  static bool close();

  void send_msg(SOCKET receiver, const char *msg, int len);
  int num_clients_connected();

private:
  static WebSocketServer *_instance;

  WebSocketThread _thread;
};

#define WEBSOCKET_SERVER WebSocketServer::instance()

#endif
