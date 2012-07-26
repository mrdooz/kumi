#pragma once
#include "json_utils.hpp"
#include "utils.hpp"

struct ProfileScope {
  ProfileScope(const char *name);
  ~ProfileScope();

  const char *name;
  DWORD thread_id;
};

class ProfileManager {
public:
  static ProfileManager &instance();
  static bool create();
  static bool close();

  void start_frame();
  JsonValue::JsonValuePtr end_frame();

  void enter_scope(ProfileScope *scope);
  void leave_scope(ProfileScope *scope);
private:

  ProfileManager();
  ~ProfileManager();

  struct TimelineEvent {
    TimelineEvent(const char *name, LARGE_INTEGER start, int cur_level, int parent) 
      : name(name), start(start), cur_level(cur_level), parent(parent) {}
    const char *name;
    LARGE_INTEGER start, end;
    int cur_level;
    int parent;
  };

  struct Timeline {
    Timeline(int thread_id, const char *thread_name, LARGE_INTEGER start) 
      : max_depth(0), thread_id(thread_id), thread_name(thread_name), start_time(start) {}
    int max_depth;
    int thread_id;
    const char *thread_name;
    LARGE_INTEGER start_time;
    std::stack<int> callstack;
    std::vector<TimelineEvent> events;
  };

  std::unordered_map<DWORD, Timeline *> _timeline;

  CriticalSection _callstack_cs;

  LARGE_INTEGER _frequency;
  LARGE_INTEGER _frame_start, _frame_end;
  static ProfileManager *_instance;
};

#define PROFILE_MANAGER ProfileManager::instance()
#define ADD_PROFILE_SCOPE() ProfileScope GEN_NAME(PROFILE, __LINE__)(__FUNCTION__);
#define ADD_NAMED_PROFILE_SCOPE(name) ProfileScope GEN_NAME(PROFILE, __LINE__)(name);