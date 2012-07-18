#include "stdafx.h"
#include "animation_manager.hpp"
#include "property_manager.hpp"

using namespace std;

AnimationManager *AnimationManager::_instance;

bool AnimationManager::create() {
  KASSERT(!_instance);
  _instance = new AnimationManager;
  return true;
}

bool AnimationManager::close() {
  KASSERT(_instance);
  delete exch_null(_instance);
  return true;
}

AnimationManager::AnimationManager() {

}

AnimationManager &AnimationManager::instance() {
  KASSERT(_instance);
  return *_instance;
}


void *AnimationManager::alloc_anim(const std::string &name, AnimationType type, int frames) {

  PropertyId id = PROPERTY_MANAGER.get_or_create<AnimData>(name + "::Anim");
  AnimData *anim_data = PROPERTY_MANAGER.get_property_ptr<AnimData>(id);

  switch (type) {

    case kAnimPos: {
      auto kf = new KeyFrameCacheVec3(frames);
      anim_data->pos = kf;
      _vec3_keyframes.push_back(kf);
      return kf->keyframes;
    }

    case kAnimScale: {
      auto kf = new KeyFrameCacheVec3(frames);
      anim_data->scale = kf;
      _vec3_keyframes.push_back(kf);
      return kf->keyframes;
    }

    case kAnimRot: {
      auto kf = new KeyFrameCacheVec4(frames);
      anim_data->rot = kf;
      _vec4_keyframes.push_back(kf);
      return kf->keyframes;
    }
  }

  KASSERT(false);
  return nullptr;
}

template<class T>
void AnimationManager::update_inner(double time, vector<KeyFrameCache<T> *> *keyframes) {
  for (size_t i = 0; i < keyframes->size(); ++i) {
    auto *p = (*keyframes)[i];
    auto *frames = p->keyframes;
    int start_frame = p->prev_frame;
    if (time < p->prev_time) {
      // backwards in time, so all bets are off
      start_frame = 0;
      p->prev_frame = 0;
    }
    p->prev_time = time;

    int prev_frame = p->prev_frame;
    if (prev_frame == p->num_frames - 1) {
      // At the last frame, so we don't have to update anything..
    } else {
      int cur_frame = start_frame;
      while (time > frames[cur_frame].time && cur_frame < p->num_frames - 1)
        cur_frame++;

      if (cur_frame > 0) {
        if (cur_frame < p->num_frames - 1) {
          auto a = frames[cur_frame-1].value;
          auto b = frames[cur_frame].value;
          double at = frames[cur_frame-1].time;
          double bt = frames[cur_frame].time;
          double t = (time - at) / (bt - at);
          p->cur = lerp(a, b, (float)t);
          p->prev_frame = cur_frame;

        } else {
          p->prev_frame = p->num_frames - 1;
          p->cur = frames[p->prev_frame].value;
        }
      }
    }
  }

}

void AnimationManager::update(double time) {
  update_inner(time, &_vec3_keyframes);
  update_inner(time, &_vec4_keyframes);
}

XMFLOAT4X4 AnimationManager::get_xform(PropertyId id, bool transpose) {

  AnimData anim_data = PROPERTY_MANAGER.get_property<AnimData>(id);
  XMFLOAT3 pos = anim_data.pos->cur;
  XMFLOAT4 rot = anim_data.rot->cur;
  XMFLOAT3 scale = anim_data.scale->cur;

  XMVECTOR vZero = XMLoadFloat3(&XMFLOAT3(0,0,0));
  XMVECTOR qId = XMLoadFloat4(&XMFLOAT4(0,0,0,1));
  XMVECTOR qRot = XMLoadFloat4(&rot);
  XMVECTOR vPos = XMLoadFloat3(&pos);
  XMVECTOR vScale = XMLoadFloat3(&scale);

  XMMATRIX mtx = XMMatrixTransformation(vZero, qId, vScale, vZero, qRot, vPos);
  if (transpose)
    mtx = XMMatrixTranspose(mtx);
  XMFLOAT4X4 res;
  XMStoreFloat4x4(&res, mtx);

  return res;
}

XMFLOAT3 AnimationManager::get_pos(PropertyId id) {

  AnimData anim_data = PROPERTY_MANAGER.get_property<AnimData>(id);
  return anim_data.pos->cur;
}

void AnimationManager::on_loaded() {
  // init the cache to the first frame
  for (size_t i = 0; i < _vec3_keyframes.size(); ++i) {
    auto *p = _vec3_keyframes[i];
    if (p->num_frames && p->prev_frame == -1) {
      p->cur = p->keyframes[0].value;
      p->prev_frame = 0;
      p->prev_time = p->keyframes[0].time;
    }
  }

  for (size_t i = 0; i < _vec4_keyframes.size(); ++i) {
    auto *p = _vec4_keyframes[i];
    if (p->num_frames && p->prev_frame == -1) {
      p->cur = p->keyframes[0].value;
      p->prev_frame = 0;
      p->prev_time = p->keyframes[0].time;
    }
  }

}
