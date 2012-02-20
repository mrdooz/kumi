#pragma once
#include <concurrent_queue.h>
#include "utils.hpp"

struct TrackedLocation {
  TrackedLocation() {}
  TrackedLocation(const char *function, const char *file, int line) : function(function), file(file), line(line) {}
  std::string function;
  std::string file;
  int line;
};

#define FROM_HERE TrackedLocation(__FUNCTION__, __FILE__, __LINE__)

namespace threading {
  enum ThreadId {
    kMainThread,
    kIoThread,
    kFileMonitorThread,

    kThreadCount,
  };

  struct DeferredCall {
    typedef std::function<void()> Fn;

    DeferredCall() : handle(INVALID_HANDLE_VALUE) {}
    DeferredCall(const TrackedLocation &location, HANDLE handle, const Fn &callback) 
      : location(location), handle(handle), callback(callback) {}
    DeferredCall(const TrackedLocation &location, const Fn &callback) 
      : location(location), handle(INVALID_HANDLE_VALUE), callback(callback) {}

    TrackedLocation location;
    HANDLE handle;
    Fn callback;
    //NamedCallback callback;
  };

#define NAMED_CALLBACK(x) NamedCallback(#x, x)

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
    Thread();
    virtual ~Thread();
    virtual void add_deferred(const DeferredCall &call);
    void process_deferred();

    HANDLE _thread;
    ThreadId _thread_id;
    HANDLE _cancel_event;
    Concurrency::concurrent_queue<DeferredCall> _deferred;
  };

  // These guys just process messages and then call the on_idle as fast as they can
  class GreedyThread : public Thread {
  public:
    virtual bool start();
  private:
    static UINT __stdcall run(void *data);
  };

  // Sleep threads spent a lot of time sleeping, so when a deferred call is added, we set the _deferred_event
  // to wake these guys up
  class SleepyThread : public Thread {
  public:
    virtual bool start();
  protected:
    SleepyThread();
    ~SleepyThread();
    static UINT __stdcall run(void *data);
    virtual void add_deferred(const DeferredCall &call);
    HANDLE _deferred_event;
  };

  // Sleeps for a user specific time before calling on_idle
  class CustomSleepyThread : public Thread {
  public:
    virtual bool start();
  protected:
    CustomSleepyThread();
    ~CustomSleepyThread();
    static UINT __stdcall run(void *data);
    virtual void add_deferred(const DeferredCall &call);
    HANDLE _deferred_event;
    DWORD _sleep_interval;
  };

  class Dispatcher {
  public:

    void set_thread(ThreadId id, Thread *thread);

    static Dispatcher &instance();

    // queue the function on the specified thread's queue
    void invoke(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb);
    void invoke_and_wait(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb);

  private:
    Dispatcher();

    Concurrency::critical_section _thread_cs;
    Thread *_threads[kThreadCount];
    static Dispatcher *_instance;
  };

  struct MessageLoop {
  public:
  private:

  };
}

#define DISPATCHER threading::Dispatcher::instance()
