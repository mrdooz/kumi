#include "stdafx.h"
#include "animation_manager.hpp"
#include "property_manager.hpp"

using namespace std;


// Stolen from Wm5BSplineFitBasis
static void compute_factors(float t, int num_pts, int* min_idx, int* max_idx, float *coeffs) {
  int mDegree = 3;
  float mKnot[6];

  // Use scaled time and scaled knots so that 1/(Q-D) does not need to
  // be explicitly stored by the class object.  Determine the extreme
  // indices affected by local control.
  float QmD = (float)(num_pts - mDegree);
  float tValue;
  if (t <= (float)0)
  {
    tValue = (float)0;
    *min_idx = 0;
    *max_idx = mDegree;
  }
  else if (t >= (float)1)
  {
    tValue = QmD;
    *max_idx = num_pts - 1;
    *min_idx = *max_idx - mDegree;
  }
  else
  {
    tValue = QmD*t;
    *min_idx = (int)tValue;
    *max_idx = *min_idx + mDegree;
  }

  // Precompute the knots.
  for (int i0 = 0, i1 = *max_idx+1-mDegree; i0 < 2*mDegree; ++i0, ++i1)
  {
    if (i1 <= mDegree)
    {
      mKnot[i0] = (float)0;
    }
    else if (i1 >= num_pts)
    {
      mKnot[i0] = QmD;
    }
    else
    {
      mKnot[i0] = (float)(i1 - mDegree);
    }
  }

  // Initialize the basis function evaluation table.  The first degree-1
  // entries are zero, but they do not have to be set explicitly.
  coeffs[mDegree] = (float)1;

  // Update the basis function evaluation table, each iteration overwriting
  // the results from the previous iteration.
  for (int row = mDegree-1; row >= 0; --row)
  {
    int k0 = mDegree, k1 = row;
    float knot0 = mKnot[k0], knot1 = mKnot[k1];
    float invDenom = ((float)1)/(knot0 - knot1);
    float c1 = (knot0 - tValue)*invDenom, c0;
    coeffs[row] = c1*coeffs[row + 1];

    for (int col = row + 1; col < mDegree; ++col)
    {
      c0 = (tValue - knot1)*invDenom;
      coeffs[col] *= c0;

      knot0 = mKnot[++k0];
      knot1 = mKnot[++k1];
      invDenom = ((float)1)/(knot0 - knot1);
      c1 = (knot0 - tValue)*invDenom;
      coeffs[col] += c1*coeffs[col + 1];
    }

    c0 = (tValue - knot1)*invDenom;
    coeffs[mDegree] *= c0;
  }
}


XMFLOAT4X4 compose_transform(const XMFLOAT3 &pos, const XMFLOAT4 &rot, const XMFLOAT3 &scale, bool transpose) {

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


void *AnimationManager::alloc_anim2(const std::string &name, AnimationType type, int num_control_pts) {

  PropertyId id = PROPERTY_MANAGER.get_or_create<AnimData>(name + "::Anim");
  AnimData *anim_data = PROPERTY_MANAGER.get_property_ptr<AnimData>(id);
  anim_data->spline_method = true;

  switch (type) {
    case kAnimPos: {
      anim_data->pos_spline = new SplineCacheVec3(num_control_pts);
      _vec3_splines.push_back(anim_data->pos_spline);
      return anim_data->pos_spline->control_points.data();
    }

    case kAnimScale: {
      anim_data->scale_spline = new SplineCacheVec3(num_control_pts);
      _vec3_splines.push_back(anim_data->scale_spline);
      return anim_data->scale_spline->control_points.data();
    }

    case kAnimRot: {
      anim_data->rot_spline = new SplineCacheVec4(num_control_pts);
      _vec4_splines.push_back(anim_data->rot_spline);
      return anim_data->rot_spline->control_points.data();
    }
  }

  KASSERT(false);
  return nullptr;

}

void *AnimationManager::alloc_anim(const std::string &name, AnimationType type, int frames) {

  PropertyId id = PROPERTY_MANAGER.get_or_create<AnimData>(name + "::Anim");
  AnimData *anim_data = PROPERTY_MANAGER.get_property_ptr<AnimData>(id);

  // Keep track of all the unique anim datas
  if (_anim_data_id.find(id) == _anim_data_id.end()) {
    _anim_data_id.insert(id);
    _anim_data.push_back(anim_data);
  }

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

XMFLOAT3 interpolate(const XMFLOAT3 &a, const XMFLOAT3 &b, float t) {
  return lerp(a,b,t);
}

XMFLOAT4 interpolate(const XMFLOAT4 &a, const XMFLOAT4 &b, float t) {
  // When doing quaternian lerp, we have to check the sign of the dot(a,b)
  XMFLOAT4 c = a;
  if (dot(a,b) < 0) {
    c.x = -c.x; c.y = -c.y; c.z = -c.z; c.w = -c.w;
  }
  return lerp(c, b, t);
}

template<class T>
void AnimationManager::update_inner(double time, 
  vector<KeyFrameCache<T> *> *keyframes) {
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
      while (time >= frames[cur_frame].time && cur_frame < p->num_frames - 1)
        cur_frame++;

      if (cur_frame > 0) {
        if (cur_frame < p->num_frames - 1) {
          auto a = frames[cur_frame-1].value;
          auto b = frames[cur_frame].value;
          double at = frames[cur_frame-1].time;
          double bt = frames[cur_frame].time;
          double t = (time - at) / (bt - at);
          KASSERT(t >= 0 && t < 1);
          p->cur = interpolate(a, b, (float)t);
          p->prev_frame = cur_frame;

        } else {
          p->prev_frame = p->num_frames - 1;
          p->cur = frames[p->prev_frame].value;
        }
      }
    }
  }
}

static void zero_vector(XMFLOAT3 *v) {
  v->x = v->y = v->z = 0;
}

static void zero_vector(XMFLOAT4 *v) {
  v->x = v->y = v->z = v->w = 0;
}

template<typename T>
void update_spline(float t, const vector<T> &control_pts, T *cur) {
  float coeffs[4];
  int min_idx, max_idx;
  compute_factors(t, control_pts.size(), &min_idx, &max_idx, coeffs);

  zero_vector(cur);
  for (int i = 0; i <= max_idx - min_idx; ++i)
    *cur = *cur + coeffs[i] * control_pts[min_idx + i];

}

void AnimationManager::update(double time) {

/*
  for (size_t i = 0; i < _anim_data.size(); ++i) {
    AnimData *cur = _anim_data[i];
  }
*/
  update_inner<XMFLOAT3>(time, &_vec3_keyframes);
  update_inner<XMFLOAT4>(time, &_vec4_keyframes);

  for (size_t i = 0; i < _vec3_splines.size(); ++i) {
    update_spline((float)time / 100, _vec3_splines[i]->control_points, &_vec3_splines[i]->cur);
  }

  for (size_t i = 0; i < _vec4_splines.size(); ++i) {
    update_spline((float)time / 100, _vec4_splines[i]->control_points, &_vec4_splines[i]->cur);
  }

}

void AnimationManager::get_xform(PropertyId id, uint32 *flags, PosRotScale *prs) {

  *flags = 0;
  AnimData anim_data = PROPERTY_MANAGER.get_property<AnimData>(id);

  if (anim_data.spline_method) {

    if (anim_data.pos_spline) {
      prs->pos = anim_data.pos_spline->cur;
      *flags |= kAnimPos;
    }

    if (anim_data.rot_spline) {
      prs->rot = anim_data.rot_spline->cur;
      *flags |= kAnimRot;
    }

    if (anim_data.scale_spline) {
      prs->scale = anim_data.scale_spline->cur;
      *flags |= kAnimScale;
    }

  } else {

    if (anim_data.pos) {
      prs->pos = anim_data.pos->cur;
      *flags |= kAnimPos;
    }

    if (anim_data.rot) {
      prs->rot = anim_data.rot->cur;
      *flags |= kAnimRot;
    }

    if (anim_data.scale) {
      prs->scale = anim_data.scale->cur;
      *flags |= kAnimScale;
    }
  }
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
  if (anim_data.pos)
    return anim_data.pos->cur;
  else if (anim_data.pos_spline)
    return anim_data.pos_spline->cur;
  return XMFLOAT3(0,0,0);
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
