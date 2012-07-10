#pragma once
#include <concurrent_queue.h>
#include "utils.hpp"
#include "tracked_location.hpp"

namespace threading {

  enum ThreadId {
    kMainThread,
    kIoThread,
    kFileMonitorThread,
    kWebSocketThread,

    kThreadCount,
  };

  struct DeferredCall {
    typedef std::function<void()> Fn;

    DeferredCall() : handle(INVALID_HANDLE_VALUE) {}
    DeferredCall(const TrackedLocation &location, HANDLE handle, const Fn &callback) 
      : location(location), handle(handle), invoke_at(~0), callback(callback) {}
    DeferredCall(const TrackedLocation &location, const Fn &callback) 
      : location(location), handle(INVALID_HANDLE_VALUE), invoke_at(~0), callback(callback) {}
    DeferredCall(const TrackedLocation &location, DWORD invoke_at, const Fn &callback) 
      : location(location), handle(INVALID_HANDLE_VALUE), invoke_at(invoke_at), callback(callback) {}

    TrackedLocation location;
    HANDLE handle;
    DWORD invoke_at;
    Fn callback;
  };

  class Thread {
  public:
    friend class Dispatcher;

    bool join();
    bool started() const;

    virtual bool start() = 0;
    // a thread's run loop consists of processing its messages, and then calling the on_idle handler
    virtual void on_idle() = 0;
    virtual void has_joined() {}

    ThreadId thread_id() const;
  protected:
    DISALLOW_COPY_AND_ASSIGN(Thread);

    Thread(ThreadId thread_id);
    virtual ~Thread();
    virtual void add_deferred(const DeferredCall &call);
    void process_deferred();

    DWORD _thread_start;
    HANDLE _thread;
    ThreadId _thread_id;
    HANDLE _cancel_event;
    HANDLE _callbacks_added;
    Concurrency::concurrent_queue<DeferredCall> _deferred;
    int _ping_pong_idx;
    Concurrency::concurrent_queue<DeferredCall> _deferred_ping_pong[2];
  };

  // These guys just process messages and then call the on_idle as fast as they can
  class GreedyThread : public Thread {
  public:
    GreedyThread(ThreadId thread_id) : Thread(thread_id) {}
    virtual bool start();
  private:
    static UINT __stdcall run(void *data);
  };

  // Sleeps for a user specific time before calling on_idle
  class SleepyThread : public Thread {
  public:
    virtual bool start();
  protected:
    SleepyThread(ThreadId thread_id);
    ~SleepyThread();
    static UINT __stdcall run(void *data);
    virtual void add_deferred(const DeferredCall &call);
    HANDLE _deferred_event;
    DWORD _sleep_interval;
  };

  class BlockingThread : public Thread {
  public:
    virtual bool start();
  protected:
    BlockingThread(ThreadId thread_id, void *user_data);
    static UINT __stdcall run(void *data);
    virtual UINT blocking_run(void *data) = 0;
    void *_user_data;
  };

  class Dispatcher {
  public:

    void set_thread(ThreadId id, Thread *thread);

    static Dispatcher &instance();
    static void close();

    // queue the function on the specified thread's queue
    void invoke(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb);
    void invoke_in(const TrackedLocation &location, ThreadId id, DWORD delta_ms, const std::function<void()> &cb);
    void invoke_and_wait(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb);

  private:
    Dispatcher();

    CriticalSection _thread_cs;
    Thread *_threads[kThreadCount];
    static Dispatcher *_instance;
  };

  struct MessageLoop {
  public:
  private:

  };
}

#define DISPATCHER threading::Dispatcher::instance()
