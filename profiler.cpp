#include "stdafx.h"
#include "profiler.hpp"
#include "utils.hpp"
#include "json_utils.hpp"

ProfileManager *ProfileManager::_instance;

ProfileManager::StackEntry::StackEntry(ProfileScope *scope)
  : scope(scope) {
  QueryPerformanceCounter(&start_time);
}

ProfileManager::ProfileManager() {
}

ProfileManager &ProfileManager::instance() {
  assert (_instance);
  return *_instance;
}

bool ProfileManager::create() {
  assert(!_instance);
  _instance = new ProfileManager();
  return true;
}

bool ProfileManager::close() {
  assert(_instance);
  delete exch_null(_instance);
  return true;
}

void ProfileManager::enter_scope(ProfileScope *scope) {
  _active_threads.insert(scope->thread_id);
  auto &callstack = _callstack[scope->thread_id];
  callstack.emplace(StackEntry(scope));
}

void ProfileManager::leave_scope(ProfileScope *scope) {
  auto &callstack = _callstack[scope->thread_id];
  auto &top = callstack.top();
  assert(top.scope == scope);
  QueryPerformanceCounter(&top.end_time);
  _timeline[scope->thread_id].emplace_back(ProfileEvent(top));
  callstack.pop();
}

void ProfileManager::start_frame() {
}

void ProfileManager::end_frame() {
  // create the json rep of the current frame

  auto &root = JsonValue::create_object();
  for (auto it = begin(_active_threads); it != end(_active_threads); ++it) {

    auto &cur_thread = JsonValue::create_object();
    DWORD thread_id = *it;
    cur_thread->add_key_value("ThreadId", (int)thread_id);
    auto &timeline = JsonValue::create_array();

    for (auto j = begin(_timeline[thread_id]); j != end(_timeline[thread_id]); ++j) {
      auto &cur_event = JsonValue::create_object();
      cur_event->add_key_value("Name", j->name);
      cur_event->add_key_value("Start", (double)j->start_time.QuadPart);
      cur_event->add_key_value("End", (double)j->end_time.QuadPart);
      timeline->add_value(cur_event);
    }
    cur_thread->add_key_value("Events", timeline);
  }

  _callstack.clear();
  _timeline.clear();
  _active_threads.clear();

}


ProfileScope::ProfileScope(const char *name)
  : name(name)
  , thread_id(GetCurrentThreadId())
{
  PROFILE_MANAGER.enter_scope(this);
}

ProfileScope::~ProfileScope() {
  PROFILE_MANAGER.leave_scope(this);
}
