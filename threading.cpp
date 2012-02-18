#include "stdafx.h"
#include "threading.hpp"
#include <concurrent_queue.h>

using std::pair;
using std::make_pair;
using boost::scoped_ptr;

namespace threading {

Dispatcher *Dispatcher::_instance = nullptr;

Dispatcher &Dispatcher::instance() {
  if (!_instance)
    _instance = new Dispatcher;
  return *_instance;
}

Dispatcher::Dispatcher() {
  ZeroMemory(_threads, sizeof(_threads));
}

void Dispatcher::invoke(const TrackedLocation &location, ThreadId id, const NamedCallback &callback) {
  SCOPED_CONC_CS(_thread_cs);
  //SCOPED_CS(_thread_cs);
  Thread *thread = _threads[id];
  if (!thread)
    return;

  thread->add_deferred(DeferredCall(location, callback));
}

void Dispatcher::invoke_and_wait(const TrackedLocation &location, ThreadId id, const NamedCallback &callback) {
  SCOPED_CONC_CS(_thread_cs);
  Thread *thread = _threads[id];
  if (!thread)
    return;

  HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
  thread->add_deferred(DeferredCall(location, h, callback));
  WaitForSingleObject(h, INFINITE);
  CloseHandle(h);
}


void Dispatcher::set_thread(ThreadId id, Thread *thread) {
  SCOPED_CONC_CS(_thread_cs);
  assert(!_threads[id]);
  _threads[id] = thread;
}

Thread::Thread() 
  : _thread(INVALID_HANDLE_VALUE) 
{
}

Thread::~Thread() {
}

bool Thread::started() const { 
  return _thread != INVALID_HANDLE_VALUE; 
}

ThreadId Thread::thread_id() const { 
  return _thread_id; 
}

UINT Thread::run(void *data) {
  Thread *self = (Thread *)data;

  return 0;
}

int Thread::join() {
  return 0;
}

bool Thread::start() {
  if (_thread != INVALID_HANDLE_VALUE)
    return false;

  _thread = (HANDLE)_beginthreadex(NULL, 0, &Thread::run, this, 0, NULL);
  return _thread != INVALID_HANDLE_VALUE;
}

void Thread::add_deferred(const DeferredCall &call) {
  _deferred.push(call);
}

void Thread::process_deferred() {
  while (!_deferred.empty()) {
    DeferredCall cur;
    if (_deferred.try_pop(cur)) {
      cur.callback();
      if (cur.handle != INVALID_HANDLE_VALUE)
        SetEvent(cur.handle);
    }
  }
}

SleepyThread::SleepyThread() 
  : Thread()
  , _deferred_event(CreateEvent(NULL, FALSE, FALSE, NULL))
{
}

SleepyThread::~SleepyThread() {
  CloseHandle(_deferred_event);
}

void SleepyThread::add_deferred(const DeferredCall &call) {
  _deferred.push(call);
  SetEvent(_deferred_event);
}

}
