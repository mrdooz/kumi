#pragma once
#include "scene.hpp"

#pragma pack(push, 1)
template <typename T>
struct KeyFrame {
  float time;
  T value;
};

typedef KeyFrame<float> KeyFrameFloat;
typedef KeyFrame<XMFLOAT3> KeyFrameVec3;
typedef KeyFrame<XMFLOAT4> KeyFrameVec4;
typedef KeyFrame<XMFLOAT4X4> KeyFrameMatrix;

#pragma pack(pop)

XMFLOAT4X4 compose_transform(const XMFLOAT3 &pos, const XMFLOAT4 &rot, const XMFLOAT3 &scale, bool transpose);

struct PosRotScale {
  XMFLOAT3 pos;
  XMFLOAT4 rot;
  XMFLOAT3 scale;
};

class AnimationManager {
public:

  enum AnimationType {
    kAnimPos    = (1 << 0),
    kAnimRot    = (1 << 1),
    kAnimScale  = (1 << 2),
  };

  static AnimationManager &instance();
  static bool create();
  static bool close();

  void update(double time);
  void *alloc_anim(const std::string &name, AnimationType type, int frames);
  void *alloc_anim2(const std::string &name, AnimationType type, int num_control_pts);
  void on_loaded();

  void get_xform(PropertyId id, uint32 *flags, PosRotScale *prs);

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

  template<typename T>
  struct SplineCache {
    SplineCache(int num_control_pts) : control_points(num_control_pts) {}
    std::vector<T> control_points;
    T cur;
  };

  typedef KeyFrameCache<XMFLOAT3> KeyFrameCacheVec3;
  typedef KeyFrameCache<XMFLOAT4> KeyFrameCacheVec4;

  typedef SplineCache<XMFLOAT3> SplineCacheVec3;
  typedef SplineCache<XMFLOAT4> SplineCacheVec4;

  // TODO: fix memory leak
  struct AnimData {
    AnimData()
      : pos(nullptr), rot(nullptr), scale(nullptr), pos_spline(nullptr), rot_spline(nullptr), scale_spline(nullptr), spline_method(false) {}
    XMFLOAT4X4 cur_mtx;
    KeyFrameCacheVec3 *pos;
    KeyFrameCacheVec4 *rot;
    KeyFrameCacheVec3 *scale;

    SplineCacheVec3 *pos_spline;
    SplineCacheVec4 *rot_spline;
    SplineCacheVec3 *scale_spline;

    bool spline_method;
  };

  AnimationManager();

  template<class T> void update_inner(double time, std::vector<KeyFrameCache<T> *> *keyframes);

  std::vector<KeyFrameCacheVec3 *> _vec3_keyframes;
  std::vector<KeyFrameCacheVec4 *> _vec4_keyframes;

  std::vector<SplineCacheVec3 *> _vec3_splines;
  std::vector<SplineCacheVec4 *> _vec4_splines;

  std::vector<AnimData *> _anim_data;
  std::set<PropertyId> _anim_data_id;
};

#define ANIMATION_MANAGER AnimationManager::instance()