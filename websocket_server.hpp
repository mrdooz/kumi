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
private:
  std::vector<HANDLE> _client_threads;
};

class WebSocketServer {
public:
  static WebSocketServer& instance();
  static bool create();
  static bool close();

  void send_msg(SOCKET receiver, const char *msg, int len);

private:
  static WebSocketServer *_instance;

  WebSocketThread _thread;
};

#define WEBSOCKET_SERVER WebSocketServer::instance()

#endif
