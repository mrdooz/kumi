#include "stdafx.h"
#include "threading.hpp"
#include <concurrent_queue.h>

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

Dispatcher::Dispatcher() {
  ZeroMemory(_threads, sizeof(_threads));
}

void Dispatcher::invoke(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb) {
  SCOPED_CONC_CS(_thread_cs);
  //SCOPED_CS(_thread_cs);
  Thread *thread = _threads[id];
  if (!thread)
    return;

  thread->add_deferred(DeferredCall(location, cb));
}

void Dispatcher::invoke_and_wait(const TrackedLocation &location, ThreadId id, const std::function<void()> &cb) {
  SCOPED_CONC_CS(_thread_cs);
  Thread *thread = _threads[id];
  if (!thread)
    return;

  HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
  thread->add_deferred(DeferredCall(location, h, cb));
  WaitForSingleObject(h, INFINITE);
  CloseHandle(h);
}


void Dispatcher::set_thread(ThreadId id, Thread *thread) {
  SCOPED_CONC_CS(_thread_cs);
  assert(!_threads[id]);
  _threads[id] = thread;
}

//////////////////////////////////////////////////////////////////////////
//
// Thread
//
//////////////////////////////////////////////////////////////////////////
Thread::Thread() 
  : _thread(INVALID_HANDLE_VALUE) 
  , _cancel_event(CreateEvent(NULL, FALSE, FALSE, NULL))
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

bool Thread::join() {
  if (_thread == INVALID_HANDLE_VALUE)
    return false;

  if (!SetEvent(_cancel_event))
    return false;

  if (WaitForSingleObject(_thread, INFINITE) != WAIT_OBJECT_0)
    return false;

  _thread = INVALID_HANDLE_VALUE;
  return true;
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
SleepyThread::SleepyThread() 
  : Thread()
  , _deferred_event(CreateEvent(NULL, FALSE, FALSE, NULL))
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
  while ((res = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE) != WAIT_OBJECT_0)) {
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
// CustomSleepyThread
//
//////////////////////////////////////////////////////////////////////////
CustomSleepyThread::CustomSleepyThread() 
  : Thread()
  , _deferred_event(CreateEvent(NULL, FALSE, FALSE, NULL))
  , _sleep_interval(INFINITE)
{
}

CustomSleepyThread::~CustomSleepyThread() {
  CloseHandle(_deferred_event);
}

bool CustomSleepyThread::start() {
  if (_thread != INVALID_HANDLE_VALUE)
    return false;

  _thread = (HANDLE)_beginthreadex(NULL, 0, &CustomSleepyThread::run, this, 0, NULL);
  return _thread != INVALID_HANDLE_VALUE;
}

UINT CustomSleepyThread::run(void *data) {
  CustomSleepyThread *self = (CustomSleepyThread *)data;

  HANDLE events[] = { self->_cancel_event, self->_deferred_event };
  DWORD res;
  while ((res = WaitForMultipleObjectsEx(ARRAYSIZE(events), events, FALSE, self->_sleep_interval, TRUE) 
         != WAIT_OBJECT_0)) {
    self->process_deferred();
    self->on_idle();
  }

  return 0;
}

void CustomSleepyThread::add_deferred(const DeferredCall &call) {
  _deferred.push(call);
  SetEvent(_deferred_event);
}

}
