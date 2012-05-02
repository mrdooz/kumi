#pragma once
#if WITH_WEBSOCKETS

#include "threading.hpp"
class WebSocketThread : public threading::BlockingThread {

public:
  WebSocketThread();
  void stop();

protected:
  virtual void on_idle() {}
  UINT blocking_run(void *data);
  struct Impl;
  Impl *_impl;
};

#endif
