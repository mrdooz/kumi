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

  struct NamedCallback {
    typedef std::function<void()> Fn;
    NamedCallback() {}
    NamedCallback(const std::string &name, const Fn &fn) : name(name), fn(fn) {}
    void operator()() { fn(); }
    std::string name;
    Fn fn;
  };

  struct DeferredCall {
    DeferredCall() : handle(INVALID_HANDLE_VALUE) {}
    DeferredCall(const TrackedLocation &location, HANDLE handle, const NamedCallback &callback) 
      : location(location), handle(handle), callback(callback) {}
    DeferredCall(const TrackedLocation &location, const NamedCallback &callback) 
      : location(location), handle(INVALID_HANDLE_VALUE), callback(callback) {}
    TrackedLocation location;
    HANDLE handle;
    NamedCallback callback;
  };

#define NAMED_CALLBACK(x) NamedCallback(#x, x)

  class Thread {
  public:
    friend class Dispatcher;

    bool start();
    int join();    // joining will also delete the thread
    bool started() const;

    // a thread's run loop consists of processing its messages, and then calling the on_idle handler
    virtual void on_idle() = 0;

    ThreadId thread_id() const;
  protected:
    Thread();
    virtual ~Thread();
    static UINT __stdcall run(void *data);
    virtual void add_deferred(const DeferredCall &call);
    void process_deferred();

    HANDLE _thread;
    ThreadId _thread_id;
    HANDLE _quit_event;
    Concurrency::concurrent_queue<DeferredCall> _deferred;
  };

  // Sleep threads spent a lot of time sleeping, so when a deferred call is added, we set the _deferred_event
  // to wake these guys up
  class SleepyThread : public Thread {
  protected:
    SleepyThread();
    ~SleepyThread();
  protected:
    virtual void add_deferred(const DeferredCall &call);
    HANDLE _deferred_event;
  };

  class Dispatcher {
  public:

    void set_thread(ThreadId id, Thread *thread);

    static Dispatcher &instance();

    // queue the function on the specified thread's queue
    void invoke(const TrackedLocation &location, ThreadId id, const NamedCallback &df);
    void invoke_and_wait(const TrackedLocation &location, ThreadId id, const NamedCallback &df);

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
