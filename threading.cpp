#include "stdafx.h"
#include "threading.hpp"
#include "profiler.hpp"

using std::pair;
using std::make_pair;
using boost::scoped_ptr;

namespace threading {

//////////////////////////////////////////////////////////////////////////
//
// Dispatcher
//
//////////////////////////////////////////////////////////////////////////
Dispatcher *Dispatcher::_instance = nullptr;

Dispatcher &Dispatcher::instance() {
  if (!_instance)
    _instance = new Dispatcher;
  return *_instance;
}

void Dispatcher::close() {
  delete exch_null(_instance);
}

Dispatcher::Dispatcher() {
  ZeroMemory(_threads, sizeof(_threads));
}

void Dispatcher::invoke(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb) {
  SCOPED_CS(_thread_cs);
  Thread *thread = _threads[id];
  assert(thread);
  if (!thread)
    return;

  thread->add_deferred(DeferredCall(location, cb));
}

void Dispatcher::invoke_in(const TrackedLocation &location, ThreadId id, DWORD delta_ms, const std::function<void()> &cb) {
  SCOPED_CS(_thread_cs);
  Thread *thread = _threads[id];
  assert(thread);
  if (!thread)
    return;

  DWORD now = timeGetTime();
  DWORD t = now + delta_ms;
  if (t == ~0)
    t++;

  thread->add_deferred(DeferredCall(location, t, cb));
}

void Dispatcher::invoke_and_wait(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb) {
  SCOPED_CS(_thread_cs);
  Thread *thread = _threads[id];
  if (!thread)
    return;

  HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
  thread->add_deferred(DeferredCall(location, h, cb));
  WaitForSingleObject(h, INFINITE);
  CloseHandle(h);
}


void Dispatcher::set_thread(ThreadId id, Thread *thread) {
  SCOPED_CS(_thread_cs);
  assert(!_threads[id]);
  _threads[id] = thread;
}

//////////////////////////////////////////////////////////////////////////
//
// Thread
//
//////////////////////////////////////////////////////////////////////////
Thread::Thread(ThreadId thread_id) 
  : _thread(INVALID_HANDLE_VALUE) 
  , _cancel_event(CreateEvent(NULL, TRUE, FALSE, NULL))
  , _callbacks_added(CreateEvent(NULL, TRUE, FALSE, NULL))
  , _thread_id(thread_id)
  , _thread_start(timeGetTime())
  , _ping_pong_idx(0)
{
  DISPATCHER.set_thread(thread_id, this);
}

Thread::~Thread() {
  CloseHandle(_cancel_event);
}

bool Thread::started() const { 
  return _thread != INVALID_HANDLE_VALUE; 
}

ThreadId Thread::thread_id() const { 
  return _thread_id; 
}

bool Thread::join() {
  if (_thread == INVALID_HANDLE_VALUE)
    return false;

  if (!SetEvent(_cancel_event))
    return false;

  if (WaitForSingleObject(_thread, INFINITE) != WAIT_OBJECT_0)
    return false;

  CloseHandle(_thread);
  _thread = INVALID_HANDLE_VALUE;
  return true;
}

void Thread::add_deferred(const DeferredCall &call) {
  SetEvent(_callbacks_added);
  if (call.invoke_at != ~0)
    _deferred_ping_pong[0].push(call);
  else 
    _deferred.push(call);
}

void Thread::process_deferred() {
  //ADD_PROFILE_SCOPE();

  DWORD now = timeGetTime();

  // process time-dependent callbacks
  auto &q = _deferred_ping_pong[_ping_pong_idx];
  auto &q2 = _deferred_ping_pong[_ping_pong_idx ^ 1];
  while (!q.empty()) {
    DeferredCall cur;
    if (q.try_pop(cur)) {
      if ( (int)(cur.invoke_at - now) > 0) {
        cur.callback();
      } else {
        q2.push(cur);
      }
    }
  }

  _ping_pong_idx ^= 1;

  while (!_deferred.empty()) {
    DeferredCall cur;
    if (_deferred.try_pop(cur)) {
      cur.callback();
      if (cur.handle != INVALID_HANDLE_VALUE)
        SetEvent(cur.handle);
    }
  }

  ResetEvent(_callbacks_added);
}

//////////////////////////////////////////////////////////////////////////
//
// GreedyThread
//
//////////////////////////////////////////////////////////////////////////
bool GreedyThread::start() {
  if (_thread != INVALID_HANDLE_VALUE)
    return false;

  _thread = (HANDLE)_beginthreadex(NULL, 0, &GreedyThread::run, this, 0, NULL);
  return _thread != INVALID_HANDLE_VALUE;
}

UINT GreedyThread::run(void *data) {
  GreedyThread *self = (GreedyThread *)data;

  // process deferred callbacks until we get the cancel event
  while (WaitForSingleObject(self->_cancel_event, 0) != WAIT_OBJECT_0) {
    self->process_deferred();
    self->on_idle();
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// SleepyThread
//
//////////////////////////////////////////////////////////////////////////
SleepyThread::SleepyThread(ThreadId thread_id) 
  : Thread(thread_id)
  , _deferred_event(CreateEvent(NULL, FALSE, FALSE, NULL))
  , _sleep_interval(INFINITE)
{
}

SleepyThread::~SleepyThread() {
  CloseHandle(_deferred_event);
}

bool SleepyThread::start() {
  if (_thread != INVALID_HANDLE_VALUE)
    return false;

  _thread = (HANDLE)_beginthreadex(NULL, 0, &SleepyThread::run, this, 0, NULL);
  return _thread != INVALID_HANDLE_VALUE;
}

UINT SleepyThread::run(void *data) {
  SleepyThread *self = (SleepyThread *)data;

  HANDLE events[] = { self->_cancel_event, self->_deferred_event };
  DWORD res;
  while ((res = WaitForMultipleObjectsEx(ARRAYSIZE(events), events, FALSE, self->_sleep_interval, TRUE) 
         != WAIT_OBJECT_0)) {
    self->process_deferred();
    self->on_idle();
  }

  return 0;
}

void SleepyThread::add_deferred(const DeferredCall &call) {
  _deferred.push(call);
  SetEvent(_deferred_event);
}

//////////////////////////////////////////////////////////////////////////
//
// BlockingThread
//
//////////////////////////////////////////////////////////////////////////
BlockingThread::BlockingThread(ThreadId thread_id, void *user_data)
  : Thread(thread_id)
  , _user_data(user_data)
{
}

bool BlockingThread::start() {
  if (_thread != INVALID_HANDLE_VALUE)
    return false;

  _thread = (HANDLE)_beginthreadex(NULL, 0, &BlockingThread::run, this, 0, NULL);
  return _thread != INVALID_HANDLE_VALUE;
}

UINT BlockingThread::run(void *data) {
  BlockingThread *self = (BlockingThread *)data;
  return self->blocking_run(NULL);
}

}