#include "stdafx.h"
#include "profiler.hpp"
#include "utils.hpp"
#include "json_utils.hpp"
#include "threading.hpp"

using namespace std;

ProfileManager *ProfileManager::_instance;

ProfileManager::ProfileManager() {
  QueryPerformanceFrequency(&_frequency);
}

ProfileManager::~ProfileManager() {
  assoc_delete(&_timeline);
}

ProfileManager &ProfileManager::instance() {
  KASSERT(_instance);
  return *_instance;
}

bool ProfileManager::create() {
  KASSERT(!_instance);
  _instance = new ProfileManager();
  return true;
}

bool ProfileManager::close() {
  delete exch_null(_instance);
  return true;
}

void ProfileManager::enter_scope(ProfileScope *scope) {
  SCOPED_CS(_callstack_cs);

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  DWORD thread_id = scope->thread_id;
  Timeline *timeline = nullptr;
  auto it = _timeline.find(thread_id);
  if (it == _timeline.end()) {
    timeline = new Timeline(thread_id, threading::thread_name(thread_id), now);
    _timeline.insert(make_pair(thread_id, timeline));
  } else {
    timeline = it->second;
  }

  auto &cs = timeline->callstack;
  TimelineEvent event(scope->name, now, cs.size(), cs.empty() ? -1 : cs.top());
  timeline->callstack.push(timeline->events.size());
  timeline->events.emplace_back(event);
  timeline->max_depth = max(timeline->max_depth, (int)timeline->callstack.size());

}

void ProfileManager::leave_scope(ProfileScope *scope) {
  SCOPED_CS(_callstack_cs);

  DWORD thread_id = scope->thread_id;
  // Handle the case where an event stradles a frame because it's run on a seperate thread
  auto it = _timeline.find(thread_id);
  if (it == _timeline.end())
    return;

  Timeline *timeline = it->second;
  int idx = timeline->callstack.top();
  timeline->callstack.pop();
  QueryPerformanceCounter(&timeline->events[idx].end);
}

void ProfileManager::start_frame() {
  QueryPerformanceCounter(&_frame_start);
}

JsonValue::JsonValuePtr ProfileManager::end_frame() {

  SCOPED_CS(_callstack_cs);

  // create the json rep of the current frame
  QueryPerformanceCounter(&_frame_end);

  double freq = (double)_frequency.QuadPart;

  auto &root = JsonValue::create_object();
  auto &container = JsonValue::create_object();
  root->add_key_value("system.profile", container);
  auto &threads = JsonValue::create_array();
  container->add_key_value("startTime", _frame_start.QuadPart / freq);
  container->add_key_value("endTime", _frame_end.QuadPart / freq);
  container->add_key_value("threads", threads);

  for (auto it = begin(_timeline); it != end(_timeline); ++it) {

    auto &cur_thread = JsonValue::create_object();
    auto &tl = it->second;
    DWORD thread_id = it->first;
    cur_thread->add_key_value("threadId", (int)thread_id);
    cur_thread->add_key_value("threadName", tl->thread_name);
    cur_thread->add_key_value("maxDepth", tl->max_depth);
    auto &timeline = JsonValue::create_array();

    for (auto j = begin(tl->events); j != end(tl->events); ++j) {
      auto &cur_event = JsonValue::create_object();
      cur_event->add_key_value("name", j->name);
      cur_event->add_key_value("start", j->start.QuadPart / freq);
      cur_event->add_key_value("end", j->end.QuadPart / freq);
      cur_event->add_key_value("level", j->cur_level);
      timeline->add_value(cur_event);
    }
    cur_thread->add_key_value("events", timeline);
    threads->add_value(cur_thread);
  }

  assoc_delete(&_timeline);

  return root;
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
