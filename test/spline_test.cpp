#include "stdafx.h"
#include "spline_test.hpp"
#include "../logger.hpp"
#include "../kumi_loader.hpp"
#include "../resource_interface.hpp"
#include "../scene.hpp"
#include "../threading.hpp"
#include "../mesh.hpp"
#include "../tweakable_param.hpp"
#include "../graphics.hpp"
#include "../material.hpp"
#include "../material_manager.hpp"
#include "../xmath.hpp"
#include "../profiler.hpp"
#include "../deferred_context.hpp"
#include "../animation_manager.hpp"
#include "../app.hpp"
#include "../bit_utils.hpp"
#include "../dx_utils.hpp"
#include "../vertex_types.hpp"

using namespace std;
using namespace std::tr1::placeholders;

XMFLOAT3 perpVector(const XMFLOAT3 &a) {
  // return a vector perpendicular to "a"
  float x = fabs(a.x), y = fabs(a.y), z = fabs(a.z);
  if (x <= y && x <= z) {
    return cross(a, XMFLOAT3(1,0,0));
  } else if (y <= x && y <= z) {
    return cross(a, XMFLOAT3(0,1,0));
  } else {
    return cross(a, XMFLOAT3(0,0,1));
  }
}

float evalBSpline(float t, float p0, float p1, float p2, float p3) {
  float t2 = t*t;
  float t3 = t2*t;

  XMFLOAT4 p = XMFLOAT4(p0, p1, p2, p3);

  float a = dot(XMFLOAT4(-1,  3, -3,  1), p);
  float b = dot(XMFLOAT4( 3, -6,  3,  0), p);
  float c = dot(XMFLOAT4(-3,  0,  3,  0), p);
  float d = dot(XMFLOAT4( 1,  4,  1,  0), p);

  return dot(XMFLOAT4(t3, t2, t, 1), XMFLOAT4(a, b, c, d)) / 6;
}

XMFLOAT3 evalBSpline(float t, const XMFLOAT3 &p0, const XMFLOAT3 &p1, const XMFLOAT3 &p2, const XMFLOAT3 &p3) {
  return XMFLOAT3(
    evalBSpline(t, p0.x, p1.x, p2.x, p3.x),
    evalBSpline(t, p0.y, p1.y, p2.y, p3.y),
    evalBSpline(t, p0.z, p1.z, p2.z, p3.z));
}

void stepBSpline(int pointsPerSegment, const vector<XMFLOAT3> &controlPoints, vector<XMFLOAT3> *out) {

  int numPts = (int)controlPoints.size();
  out->resize(pointsPerSegment * (numPts - 1));
  XMFLOAT3 *dst = out->data();

  for (int i = 0; i < numPts - 1; ++i) {
    auto &p0 = controlPoints[max(0, i-1)];
    auto &p1 = controlPoints[max(0, i-0)];
    auto &p2 = controlPoints[max(0, i+1)];
    auto &p3 = controlPoints[min(numPts-1, i+2)];

    for (int j = 0; j < pointsPerSegment; ++j) {
      float t = j / (float)pointsPerSegment;
      dst->x = evalBSpline(t, p0.x, p1.x, p2.x, p3.x);
      dst->y = evalBSpline(t, p0.y, p1.y, p2.y, p3.y);
      dst->z = evalBSpline(t, p0.z, p1.z, p2.z, p3.z);
      dst++;
    }
  }
}

class DynamicSpline {
public:
  static const int cRingsPerSegment = 20;
  static const int cVertsPerRing = 20;
  static const int cStaticVbSize = 1 << 20;
  static const int cDynamicSegmentBuffer = 1;

  DynamicSpline(DeferredContext *ctx, float x, float y, float z, float growthRate)
    : _curTime(0)
    , _curTop(x, y, z)
    , _curHeight(0)
    , _segmentHeight(20)
    , _growthRate(growthRate)
    , _curStaticBufferUsed(0)
    , _ctx(ctx)
  {
    _controlPoints.push_back(_curTop);

    for (int i = 0; i < 3; ++i) {
      _curTop.x += gaussianRand(0, 15);
      _curTop.y += gaussianRand(10, 2);
      _curTop.z += gaussianRand(0, 15);
      _controlPoints.push_back(_curTop);
    }
  }

  bool init() {
    _dynamicSplineVb = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_VERTEX_BUFFER, 1024 * 1024, true, nullptr);

    _dynamicVerts.reserve(100000);
    _dynamicIndices.reserve(25000);

    for (int i = 0; i < cVertsPerRing; ++i) {
      float x = sinf(2*XM_PI*i/cVertsPerRing);
      float y = 0;
      float z = cosf(2*XM_PI*i/cVertsPerRing);
      _extrudeShape.push_back(XMFLOAT3(x,y,z));
    }

    static bool firstSpline = true;
    if (firstSpline) {
      firstSpline = false;
      createIndices(100*cRingsPerSegment);

      _splineIb = GRAPHICS.create_buffer(FROM_HERE, 
        D3D11_BIND_INDEX_BUFFER, _dynamicIndices.size() * sizeof(int), false, _dynamicIndices.data());
    }

    _staticSplines.push_back(StaticSpline(GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_VERTEX_BUFFER, cStaticVbSize, true, nullptr)));

    return true;
  }

  void update(float delta) {

    float prevHeight = _curHeight;
    _curHeight += _growthRate * delta;

    float prevSegment = floorf(prevHeight / _segmentHeight);
    float curSegment = floorf(_curHeight / _segmentHeight);

    // add new segments if needed
    while (prevSegment != curSegment) {
      prevSegment++;
      _curTop.x += gaussianRand(0, 20);
      _curTop.y += gaussianRand(10, 5);
      _curTop.z += gaussianRand(0, 20);
      _controlPoints.push_back(_curTop);
    }

    const int numPts = _controlPoints.size();

    // start recreating vertices at the first segment containing the spike
    float spikeLength = 10;
    int startingSegment = (int)(max(0,(_curHeight - spikeLength)) / _segmentHeight);

    // if we have enough static segments, dump them to the static vb
    if (startingSegment > cDynamicSegmentBuffer) {
      auto *staticVb = &_staticSplines.back();
      const int numVertsToStash = cDynamicSegmentBuffer * cRingsPerSegment * cVertsPerRing;
      if (staticVb->numVerts + numVertsToStash > cStaticVbSize) {
        // allocate a new static vb
        _staticSplines.push_back(StaticSpline(GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_VERTEX_BUFFER, cStaticVbSize, true, nullptr)));
        staticVb = &_staticSplines.back();
      }
      D3D11_MAPPED_SUBRESOURCE res;
      bool newVb = staticVb->numVerts == 0;
      _ctx->map(staticVb->vb, 0, newVb ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &res);
      //memcpy((char *)res.pData + staticVb->numVerts * sizeof(PosNormal), _dynamicVerts.data(), numVertsToStash * sizeof(PosNormal));
      _ctx->unmap(staticVb->vb, 0);
      staticVb->numVerts += numVertsToStash;

      int vertsLeft = _dynamicVerts.size() - numVertsToStash;
      vector<PosNormal> newVerts(vertsLeft);
      copy(begin(_dynamicVerts) + numVertsToStash, end(_dynamicVerts), begin(newVerts));

      _dynamicVerts.resize(vertsLeft);
      move(begin(newVerts), end(newVerts), begin(_dynamicVerts));
    } else {
      _dynamicVerts.resize(startingSegment * cRingsPerSegment * cVertsPerRing);
    }

    float c = startingSegment * _segmentHeight;

    // calc initial reference frame
    XMFLOAT3 *pts = &_controlPoints[0];
    XMFLOAT3 p0 = evalBSpline(0, pts[0], pts[1], pts[2], pts[3]);
    XMFLOAT3 p1 = evalBSpline(0.1f, pts[0], pts[1], pts[2], pts[3]);

    XMFLOAT3 t = normalize(p1 - p0);
    XMFLOAT3 n = perpVector(t);
    XMFLOAT3 b = cross(t, n);
    _prevB = b;

    float maxRadius = 3;
    float minRadius = 0.01f;
    while (c < _curHeight) {

      int idx = (int)(c / cRingsPerSegment);
      auto &p0 = _controlPoints[max(0, idx-1)];
      auto &p1 = _controlPoints[max(0, idx-0)];
      auto &p2 = _controlPoints[min(numPts-1, idx+1)];
      auto &p3 = _controlPoints[min(numPts-1, idx+2)];

      float step = 1.0f / cRingsPerSegment;
      float maxT = min(1, (_curHeight - c) / _segmentHeight);

      const auto &addRingHelper = [&](float t){
        auto pt0 = evalBSpline(t, p0, p1, p2, p3);
        auto pt1 = evalBSpline(t + 0.01f, p0, p1, p2, p3);

        auto dir = normalize(pt1-pt0);
        auto n = cross(dir, _prevB);
        auto b = cross(n, dir);
        addRing(pt0, n, dir, b, maxRadius * min(1, max(minRadius, (_curHeight - c) / spikeLength)));
      };

      for (int j = 0; j < cRingsPerSegment; ++j) {
        float t = j * step;
        addRingHelper(t);
        _prevB = b;
        c += _segmentHeight * step;
        if (c >= _curHeight) {
          // if we exceed the max height with this segment, add a cap ring
          addRingHelper(maxT);
          break;
        }
      }
    }
  }

  void render() {

    _ctx->set_ib(_splineIb, DXGI_FORMAT_R32_UINT);
/*
    // draw the static vbs
    for (size_t i = 0; i < _staticSplines.size(); ++i) {
      auto &cur = _staticSplines[i];
      if (cur.numVerts > 0) {
        _ctx->set_vb(cur.vb, sizeof(PosNormal));
        int numIndices = 3 * (cur.numVerts / cVertsPerRing - 1) * 2 * cVertsPerRing;
        _ctx->draw_indexed(numIndices, 0, 0);
      }
    }
*/

    // draw the dynamic vbs

    int numVerts = _dynamicVerts.size();
    if (!numVerts)
      return;

    D3D11_MAPPED_SUBRESOURCE res;
    _ctx->map(_dynamicSplineVb, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
    memcpy(res.pData, _dynamicVerts.data(), numVerts * sizeof(PosNormal));
    _ctx->unmap(_dynamicSplineVb, 0);

    _ctx->set_vb(_dynamicSplineVb, sizeof(PosNormal));

    int numIndices = 3 * (numVerts / cVertsPerRing - 1) * 2 * cVertsPerRing;

    _ctx->draw_indexed(numIndices, 0, 0);
  }

private:

  void createIndices(int numRings) {
    for (int i = 0; i < numRings - 1; ++i) {

      for (int j = 0; j < cVertsPerRing; ++j) {

        int prev = j == 0 ? cVertsPerRing - 1 : j - 1;

        int v0 = i*cVertsPerRing + j;
        int v1 = (i+1)*cVertsPerRing + j;
        int v2 = (i+1)*cVertsPerRing + prev;
        int v3 = i*cVertsPerRing + prev;

        _dynamicIndices.push_back(v0);
        _dynamicIndices.push_back(v1);
        _dynamicIndices.push_back(v2);

        _dynamicIndices.push_back(v0);
        _dynamicIndices.push_back(v2);
        _dynamicIndices.push_back(v3);
      }
    }
  }

  void addRing(const XMFLOAT3 &p, const XMFLOAT3 &x, const XMFLOAT3 &y, const XMFLOAT3 &z, float r) {

    XMFLOAT4X4 mtx;
    // Create transform from reference frame to world
    set_row(expand(x, 0), 0, &mtx);
    set_row(expand(y, 0), 1, &mtx);
    set_row(expand(z, 0), 2, &mtx);
    set_row(expand(p, 1), 3, &mtx);

    for (int i = 0; i < cVertsPerRing; ++i) {
      float x = r * _extrudeShape[i].x;
      float y = r * _extrudeShape[i].y;
      float z = r * _extrudeShape[i].z;
      _dynamicVerts.push_back(PosNormal(
        drop(XMFLOAT4(x,y,z,1) * mtx), 
        drop(XMFLOAT4(x,y,z,0) * mtx)));
    }
  }

  vector<int> _dynamicIndices;
  vector<PosNormal> _dynamicVerts;

  struct StaticSpline {
    StaticSpline(GraphicsObjectHandle vb) : numVerts(0), vb(vb) {}
    int numVerts;
    GraphicsObjectHandle vb;
  };

  vector<StaticSpline> _staticSplines;
  GraphicsObjectHandle _dynamicSplineVb;
  static GraphicsObjectHandle _splineIb;

  XMFLOAT3 _prevB;    // bitangent from previous iteration
  XMFLOAT3 _curTop;
  vector<XMFLOAT3> _controlPoints;
  vector<XMFLOAT3> _extrudeShape;
  int _curKnot;
  int _curSegment;
  float _segmentHeight;
  float _curHeight;
  float _growthRate;
  float _curTime;
  int _curStaticBufferUsed;

  DeferredContext *_ctx;
};

GraphicsObjectHandle DynamicSpline::_splineIb;


static float particleFade[512] = {0.0643f,0.0643f,0.0643f,0.0643f,0.0643f,0.0644f,0.0644f,0.0644f,0.0644f,0.0644f,0.0644f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0645f,0.0646f,0.0646f,0.0646f,0.0646f,0.0646f,0.0646f,0.0647f,0.0647f,0.0647f,0.0648f,0.0648f,0.0648f,0.0649f,0.0649f,0.0650f,0.0650f,0.0651f,0.0652f,0.0653f,0.0653f,0.0654f,0.0655f,0.0656f,0.0658f,0.0659f,0.0660f,0.0661f,0.0663f,0.0665f,0.0666f,0.0668f,0.0670f,0.0672f,0.0674f,0.0676f,0.0678f,0.0680f,0.0681f,0.0683f,0.0684f,0.0686f,0.0687f,0.0689f,0.0691f,0.0692f,0.0694f,0.0696f,0.0697f,0.0699f,0.0701f,0.0702f,0.0704f,0.0706f,0.0708f,0.0710f,0.0712f,0.0714f,0.0716f,0.0718f,0.0720f,0.0722f,0.0724f,0.0726f,0.0728f,0.0730f,0.0732f,0.0734f,0.0736f,0.0738f,0.0741f,0.0743f,0.0745f,0.0747f,0.0750f,0.0752f,0.0755f,0.0757f,0.0759f,0.0762f,0.0764f,0.0767f,0.0769f,0.0772f,0.0774f,0.0777f,0.0779f,0.0782f,0.0785f,0.0787f,0.0790f,0.0793f,0.0796f,0.0798f,0.0801f,0.0804f,0.0807f,0.0810f,0.0813f,0.0815f,0.0818f,0.0821f,0.0824f,0.0827f,0.0830f,0.0833f,0.0836f,0.0840f,0.0843f,0.0846f,0.0849f,0.0852f,0.0855f,0.0859f,0.0862f,0.0865f,0.0868f,0.0872f,0.0875f,0.0879f,0.0882f,0.0885f,0.0889f,0.0892f,0.0896f,0.0900f,0.0904f,0.0909f,0.0913f,0.0917f,0.0921f,0.0925f,0.0929f,0.0933f,0.0938f,0.0942f,0.0946f,0.0950f,0.0955f,0.0959f,0.0963f,0.0968f,0.0972f,0.0976f,0.0981f,0.0985f,0.0990f,0.0994f,0.0999f,0.1003f,0.1008f,0.1012f,0.1017f,0.1022f,0.1027f,0.1031f,0.1036f,0.1041f,0.1046f,0.1051f,0.1056f,0.1061f,0.1066f,0.1071f,0.1076f,0.1082f,0.1087f,0.1092f,0.1098f,0.1103f,0.1108f,0.1114f,0.1120f,0.1125f,0.1131f,0.1137f,0.1143f,0.1148f,0.1154f,0.1160f,0.1167f,0.1173f,0.1179f,0.1185f,0.1191f,0.1198f,0.1204f,0.1211f,0.1218f,0.1224f,0.1231f,0.1238f,0.1245f,0.1252f,0.1259f,0.1266f,0.1273f,0.1281f,0.1288f,0.1295f,0.1301f,0.1307f,0.1312f,0.1318f,0.1322f,0.1327f,0.1331f,0.1335f,0.1338f,0.1342f,0.1345f,0.1347f,0.1350f,0.1353f,0.1355f,0.1357f,0.1359f,0.1361f,0.1363f,0.1365f,0.1367f,0.1368f,0.1370f,0.1372f,0.1374f,0.1376f,0.1378f,0.1380f,0.1382f,0.1384f,0.1387f,0.1389f,0.1392f,0.1395f,0.1398f,0.1402f,0.1406f,0.1410f,0.1414f,0.1419f,0.1424f,0.1430f,0.1436f,0.1442f,0.1449f,0.1456f,0.1464f,0.1472f,0.1481f,0.1490f,0.1500f,0.1510f,0.1521f,0.1533f,0.1545f,0.1558f,0.1571f,0.1585f,0.1600f,0.1616f,0.1632f,0.1649f,0.1667f,0.1686f,0.1706f,0.1726f,0.1747f,0.1769f,0.1793f,0.1817f,0.1842f,0.1867f,0.1894f,0.1922f,0.1951f,0.1981f,0.2019f,0.2070f,0.2124f,0.2180f,0.2240f,0.2302f,0.2366f,0.2433f,0.2502f,0.2573f,0.2646f,0.2722f,0.2799f,0.2878f,0.2959f,0.3041f,0.3125f,0.3210f,0.3296f,0.3384f,0.3473f,0.3562f,0.3653f,0.3744f,0.3837f,0.3929f,0.4022f,0.4116f,0.4210f,0.4304f,0.4398f,0.4493f,0.4587f,0.4681f,0.4774f,0.4868f,0.4960f,0.5052f,0.5144f,0.5234f,0.5324f,0.5413f,0.5501f,0.5587f,0.5672f,0.5756f,0.5838f,0.5919f,0.5998f,0.6075f,0.6153f,0.6250f,0.6347f,0.6445f,0.6544f,0.6642f,0.6741f,0.6841f,0.6940f,0.7040f,0.7139f,0.7238f,0.7336f,0.7435f,0.7532f,0.7629f,0.7726f,0.7821f,0.7916f,0.8010f,0.8102f,0.8193f,0.8283f,0.8371f,0.8458f,0.8543f,0.8627f,0.8708f,0.8788f,0.8866f,0.8941f,0.9014f,0.9085f,0.9154f,0.9220f,0.9283f,0.9343f,0.9401f,0.9455f,0.9507f,0.9560f,0.9609f,0.9654f,0.9696f,0.9734f,0.9770f,0.9802f,0.9832f,0.9858f,0.9883f,0.9905f,0.9924f,0.9942f,0.9957f,0.9971f,0.9983f,0.9993f,1.0002f,1.0009f,1.0016f,1.0021f,1.0026f,1.0029f,1.0032f,1.0035f,1.0037f,1.0040f,1.0042f,1.0044f,1.0046f,1.0049f,1.0052f,1.0056f,1.0061f,1.0067f,1.0074f,1.0081f,1.0089f,1.0097f,1.0105f,1.0113f,1.0121f,1.0129f,1.0136f,1.0144f,1.0151f,1.0157f,1.0163f,1.0169f,1.0174f,1.0178f,1.0181f,1.0183f,1.0184f,1.0185f,1.0184f,1.0182f,1.0178f,1.0173f,1.0167f,1.0159f,1.0150f,1.0139f,1.0126f,1.0111f,1.0095f,1.0076f,1.0056f,1.0033f,1.0008f,0.9971f,0.9927f,0.9881f,0.9832f,0.9780f,0.9725f,0.9667f,0.9606f,0.9541f,0.9473f,0.9402f,0.9327f,0.9248f,0.9165f,0.9078f,0.8987f,0.8892f,0.8793f,0.8688f,0.8580f,0.8466f,0.8348f,0.8225f,0.8041f,0.7836f,0.7617f,0.7384f,0.7141f,0.6888f,0.6628f,0.6362f,0.6092f,0.5820f,0.5547f,0.5276f,0.5008f,0.4745f,0.4489f,0.4241f,0.3953f,0.3652f,0.3341f,0.3026f,0.2709f,0.2395f,0.2088f,0.1792f,0.1511f,0.1249f,0.1010f,0.0798f,0.0617f,0.0500f,0.0500f,0.0500f,0.0500f};
/*[{"x":0.5382888381545609,"y":0.06428571428571428},{"x":10.990988241659629,"y":0.06785714285714284},{"x":28.018015268686657,"y":0.0892857142857143},{"x":42.597373971786595,"y":0.12857142857142856},{"x":57.84952649331181,"y":0.19999999999999996},{"x":67.68962489429582,"y":0.6142857142857143},{"x":75.31570115505842,"y":0.95},{"x":82.32677126575953,"y":1.0071428571428571},{"x":89.09909634976773,"y":1},{"x":93.5585558092272,"y":0.8214285714285714},{"x":96.6666639173353,"y":0.42500000000000004},{"x":99.36936662003801,"y":0.050000000000000044}]*/

/*
[{"x":3.728985287899754,"y":0.8500000000000001},{"x":41.367361671663595,"y":0.8714285714285714},{"x":65.35260152406211,"y":0.8571428571428572},{"x":76.05370853513223,"y":0.6964285714285712},{"x":89.21484014644834,"y":-0.05357142857142838}]
*/

struct ParticleVtx {
  XMFLOAT3 pos;
  XMFLOAT2 scale;
};

//static const int numParticles = 5000;

// a bit of a misnomer, as we don't call T's ctor
template<typename T>
T *aligned_new(int count, int alignment) {
  return (T *)_aligned_malloc(count * sizeof(T), alignment);
}

SplineTest::SplineTest(const std::string &name) 
  : Effect(name)
  , _view_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::view"))
  , _proj_mtx_id(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::proj"))
  , _mouse_horiz(0)
  , _mouse_vert(0)
  , _mouse_lbutton(false)
  , _mouse_rbutton(false)
  , _mouse_pos_prev(~0)
  , _ctx(nullptr)
  , _useFreeFlyCamera(true)
  , _DofSettingsId(PROPERTY_MANAGER.get_or_create<XMFLOAT4X4>("System::DOFDepths"))
{
  ZeroMemory(_keystate, sizeof(_keystate));
}

SplineTest::~SplineTest() {
  seq_delete(&_splines);
  GRAPHICS.destroy_deferred_context(_ctx);
}

bool SplineTest::file_changed(const char *filename, void *token) {
  return true;
}


bool SplineTest::init() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  B_ERR_BOOL(GRAPHICS.load_techniques("effects/particles.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/scale.tec", true));
  B_ERR_BOOL(GRAPHICS.load_techniques("effects/splines.tec", true));

  _particle_technique = GRAPHICS.find_technique("particles");
  _gradient_technique = GRAPHICS.find_technique("gradient");
  _compose_technique = GRAPHICS.find_technique("compose");
  _coalesce = GRAPHICS.find_technique("coalesce");

  _splineTechnique = GRAPHICS.find_technique("SplineRender");

  _particle_texture = RESOURCE_MANAGER.load_texture("gfx/particle3.png", "", false, nullptr);

  _scale = GRAPHICS.find_technique("scale");

  _ctx = GRAPHICS.create_deferred_context(true);

  if (!_blur.init()) {
    return false;
  }

  _freefly_camera.setPos(XMFLOAT3(0, 0, -50));

  _vb = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_VERTEX_BUFFER, 1024 * 1024, true, nullptr);
  _ib = GRAPHICS.create_buffer(FROM_HERE, D3D11_BIND_INDEX_BUFFER, 1024 * 1024, true, nullptr);

  {
    TweakableParameterBlock block("blur");
    block._params.emplace_back(TweakableParameter("blurX", 10.0f, 1.0f, 250.0f));
    block._params.emplace_back(TweakableParameter("blurY", 10.0f, 1.0f, 250.0f));

    block._params.emplace_back(TweakableParameter("nearFocusStart", 10.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("nearFocusEnd", 50.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("farFocusStart", 100.0f, 1.0f, 2500.0f));
    block._params.emplace_back(TweakableParameter("farFocusEnd", 150.0f, 1.0f, 2500.0f));

    APP.add_parameter_block(block, true, [=](const JsonValue::JsonValuePtr &msg) {
      _blurX = (float)msg->get("blurX")->get("value")->to_number();
      _blurY = (float)msg->get("blurY")->get("value")->to_number();

      if (msg->has_key("nearFocusStart")) {
        _nearFocusStart = (float)msg->get("nearFocusStart")->get("value")->to_number();
        _nearFocusEnd = (float)msg->get("nearFocusEnd")->get("value")->to_number();
        _farFocusStart = (float)msg->get("farFocusStart")->get("value")->to_number();
        _farFocusEnd = (float)msg->get("farFocusEnd")->get("value")->to_number();
      }
    });
  }

  float span = 75;
#if _DEBUG
  const int numSplines = 50;
#else
  const int numSplines = 500;
#endif
  for (int i = 0; i < numSplines; ++i) {
    float x = randf(-span, span);
    float z = randf(-span, span);
    DynamicSpline *spline = new DynamicSpline(_ctx, x, 0, z, gaussianRand(10, 3));
    if (!spline->init())
      return false;
    _splines.push_back(spline);
  }

  return true;
}

void SplineTest::calc_camera_matrices(double time, double delta, XMFLOAT4X4 *view, XMFLOAT4X4 *proj) {

  *proj = transpose(_freefly_camera.projectionMatrix());

  if (_useFreeFlyCamera) {

    if (_keystate['W'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(100 * delta));
    if (_keystate['S'])
      _freefly_camera.move(FreeFlyCamera::kForward, (float)(-100 * delta));
    if (_keystate['A'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(-100 * delta));
    if (_keystate['D'])
      _freefly_camera.move(FreeFlyCamera::kRight, (float)(100 * delta));
    if (_keystate['Q'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(100 * delta));
    if (_keystate['E'])
      _freefly_camera.move(FreeFlyCamera::kUp, (float)(-100 * delta));

    if (_mouse_lbutton) {
      float dx = (float)(100 * delta) * _mouse_horiz / 200.0f;
      float dy = (float)(100 * delta) * _mouse_vert / 200.0f;
      _freefly_camera.rotate(FreeFlyCamera::kXAxis, dx);
      _freefly_camera.rotate(FreeFlyCamera::kYAxis, dy);
    }

    *view = transpose(_freefly_camera.viewMatrix());
  } else {

    XMVECTOR pos = XMLoadFloat4(&XMFLOAT4(0, 0, -50, 0));
    XMVECTOR at = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 0));
    float x = -cosf((float)time/20);
    float y = sinf((float)time/20);
    XMVECTOR up = XMLoadFloat4(&XMFLOAT4(x, y, 0, 0));
    XMMATRIX lookat = XMMatrixLookAtLH(pos, at, up);
    XMFLOAT4X4 tmp;
    XMStoreFloat4x4(&tmp, lookat);
    *view = transpose(tmp);
  }
}

bool SplineTest::update(int64 global_time, int64 local_time, int64 delta_ns, bool paused, int64 frequency, int32 num_ticks, float ticks_fraction) {
  ADD_PROFILE_SCOPE();

  double time = local_time  / 1000.0;

  for (size_t i = 0; i < _splines.size(); ++i) {
    _splines[i]->update(delta_ns / 1e6f);
  }

  calc_camera_matrices(time, delta_ns / 1e6, &_view, &_proj);

  PROPERTY_MANAGER.set_property(_view_mtx_id, _view);
  PROPERTY_MANAGER.set_property(_proj_mtx_id, _proj);

  return true;
}

void SplineTest::post_process(GraphicsObjectHandle input, GraphicsObjectHandle output, GraphicsObjectHandle technique) {
  if (output.is_valid())
    _ctx->set_render_target(output, true);
  else
    _ctx->set_default_render_target();

  TextureArray arr = { input };
  _ctx->render_technique(technique, bind(&SplineTest::fill_cbuffer, this, _1), arr, DeferredContext::InstanceData());

  if (output.is_valid())
    _ctx->unset_render_targets(0, 1);
}

void SplineTest::renderParticles() {

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

/*
  D3D11_MAPPED_SUBRESOURCE res;

  GRAPHICS.map(_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
  PosNormal *verts = (PosNormal *)res.pData;
  int numVerts = gSpline._dynamicVerts.size();
  for (int i = 0; i < numVerts; ++i) {
    verts[i].pos = gSpline._dynamicVerts[i];
    verts[i].normal = gSpline._dynamicNormals[i];
  }
  GRAPHICS.unmap(_vb, 0);


  GRAPHICS.map(_ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
  memcpy(res.pData, gSpline._dynamicIndices.data(), gSpline._dynamicIndices.size() * sizeof(int));
  GRAPHICS.unmap(_ib, 0);
*/
  Technique *technique = GRAPHICS.get_technique(_splineTechnique);
  Shader *vs = technique->vertex_shader(0);
  Shader *ps = technique->pixel_shader(0);

  _ctx->set_rs(technique->rasterizer_state());
  _ctx->set_dss(technique->depth_stencil_state(), GRAPHICS.default_stencil_ref());
  _ctx->set_bs(technique->blend_state(), GRAPHICS.default_blend_factors(), GRAPHICS.default_sample_mask());

  _ctx->set_vs(vs->handle());
  _ctx->set_ps(ps->handle());

  _ctx->set_layout(vs->input_layout());
  _ctx->set_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  struct CBuffer {
    XMFLOAT4X4 worldViewProj;
    XMFLOAT3 cameraPos;
  } cbuffer;

  XMFLOAT4X4 view = _freefly_camera.viewMatrix();
  XMFLOAT4X4 proj = _freefly_camera.projectionMatrix();

  XMFLOAT4X4 mtx;
  XMStoreFloat4x4(&mtx, XMMatrixMultiply(XMLoadFloat4x4(&view), XMLoadFloat4x4(&proj)));

  cbuffer.worldViewProj = transpose(mtx);
  cbuffer.cameraPos = _freefly_camera.pos();
  _ctx->set_cbuffer(vs->find_cbuffer("test"), 0, ShaderType::kVertexShader, &cbuffer, sizeof(cbuffer));
  _ctx->set_cbuffer(ps->find_cbuffer("test"), 0, ShaderType::kPixelShader, &cbuffer, sizeof(cbuffer));

  for (auto it = begin(_splines); it != end(_splines); ++it) {
    (*it)->render();
  }
}

bool SplineTest::render() {
  ADD_PROFILE_SCOPE();
  _ctx->begin_frame();

  int w = GRAPHICS.width();
  int h = GRAPHICS.height();

  _ctx->set_default_render_target();

  //_ctx->render_technique(_gradient_technique, 
    //bind(&SplineTest::fill_cbuffer, this, _1), TextureArray(), DeferredContext::InstanceData());

  renderParticles();

  _ctx->end_frame();

  return true;
}

bool SplineTest::close() {
  return true;
}

void SplineTest::wnd_proc(UINT message, WPARAM wParam, LPARAM lParam) {

  switch (message) {
  case WM_KEYDOWN:
    if (wParam <= 255)
      _keystate[wParam] = 1;
    break;

  case WM_KEYUP:
    if (wParam <= 255)
      _keystate[wParam] = 0;
    switch (wParam) {
    case 'F':
      //_use_freefly_camera = !_use_freefly_camera;
      break;
    }
    break;

  case WM_MOUSEMOVE:
    if (_mouse_pos_prev != ~0) {
      _mouse_horiz = LOWORD(lParam) - LOWORD(_mouse_pos_prev);
      _mouse_vert = HIWORD(lParam) - HIWORD(_mouse_pos_prev);
    }
    _mouse_pos_prev = lParam;
    break;

  case WM_LBUTTONDOWN:
    _mouse_lbutton = true;
    break;

  case WM_LBUTTONUP:
    _mouse_lbutton = false;
    break;

  case WM_RBUTTONDOWN:
    _mouse_rbutton = true;
    break;

  case WM_RBUTTONUP:
    _mouse_rbutton = false;
    break;
  }

}
