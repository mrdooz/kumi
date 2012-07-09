#pragma once

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
  void end_frame();

  void enter_scope(ProfileScope *scope);
  void leave_scope(ProfileScope *scope);
private:

  ProfileManager();

  struct StackEntry {
    StackEntry(ProfileScope *scope);
    ProfileScope *scope;
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
  };

  struct ProfileEvent {
    ProfileEvent(const StackEntry &e) : name(e.scope->name), start_time(e.start_time), end_time(e.end_time) {}
    const char *name;
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
  };

  std::unordered_map<DWORD, std::stack<StackEntry>> _callstack;
  std::unordered_map<DWORD, std::vector<ProfileEvent>> _timeline;
  std::set<DWORD> _active_threads;

  static ProfileManager *_instance;
};

#define PROFILE_MANAGER ProfileManager::instance()
#define ADD_PROFILE_SCOPE() ProfileScope GEN_NAME(PROFILE, __LINE__)(__FUNCTION__);
#define ADD_NAMED_PROFILE_SCOPE(name) ProfileScope GEN_NAME(PROFILE, __LINE__)();