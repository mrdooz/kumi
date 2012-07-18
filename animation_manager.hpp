#pragma once
#include "scene.hpp"

class AnimationManager {
public:

  enum AnimationType {
    kAnimPos,
    kAnimRot,
    kAnimScale,
  };

  static AnimationManager &instance();
  static bool create();
  static bool close();

  void update(double time);
  void *alloc_anim(const std::string &name, AnimationType type, int frames);
  void on_loaded();

  XMFLOAT4X4 get_xform(PropertyId id, bool transpose = false);
  XMFLOAT3 get_pos(PropertyId id);

private:
  static AnimationManager *_instance;

  template<typename T>
  struct KeyFrameCache {
    KeyFrameCache(int frames) : cur(T()), prev_time(-1), num_frames(frames), prev_frame(-1), keyframes(new KeyFrame<T>[frames]) {}
    T cur;
    double prev_time;
    int num_frames;
    int prev_frame;
    KeyFrame<T> *keyframes;
  };

  typedef KeyFrameCache<XMFLOAT3> KeyFrameCacheVec3;
  typedef KeyFrameCache<XMFLOAT4> KeyFrameCacheVec4;

  struct AnimData {
    AnimData() : pos(nullptr), rot(nullptr), scale(nullptr) {}
    KeyFrameCache<XMFLOAT3> *pos;
    KeyFrameCache<XMFLOAT4> *rot;
    KeyFrameCache<XMFLOAT3> *scale;
  };

  AnimationManager();

  template<class T> void update_inner(double time, std::vector<KeyFrameCache<T> *> *);

  std::vector<KeyFrameCacheVec3 *> _vec3_keyframes;
  std::vector<KeyFrameCacheVec4 *> _vec4_keyframes;
};

#define ANIMATION_MANAGER AnimationManager::instance()