#pragma once
#include "scene.hpp"

#pragma pack(push, 1)
template <typename T>
struct KeyFrame {
  double time;
  T value;
};

typedef KeyFrame<float> KeyFrameFloat;
typedef KeyFrame<XMFLOAT3> KeyFrameVec3;
typedef KeyFrame<XMFLOAT4> KeyFrameVec4;
typedef KeyFrame<XMFLOAT4X4> KeyFrameMatrix;

#pragma pack(pop)

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
    KeyFrameCacheVec3 *pos;
    KeyFrameCacheVec4 *rot;
    KeyFrameCacheVec3 *scale;
    XMFLOAT4X4 cur_mtx;
  };

  AnimationManager();

  template<class T> void update_inner(double time, std::vector<KeyFrameCache<T> *> *keyframes);

  std::vector<KeyFrameCacheVec3 *> _vec3_keyframes;
  std::vector<KeyFrameCacheVec4 *> _vec4_keyframes;

  std::vector<AnimData *> _anim_data;
  std::set<PropertyId> _anim_data_id;
};

#define ANIMATION_MANAGER AnimationManager::instance()