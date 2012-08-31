#include "common.hlsl"

cbuffer cbPlane {
  matrix WorldViewProj;
  matrix MirrorViewProj;
};

struct VsInput {
  float3 pos : POSITION;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float4 tex : TEXCOORD0;
  float3 wsPos : TEXCOORD1;
};

PsInput vs_main(VsInput input) {
  PsInput output;
  output.pos = mul(float4(input.pos, 1), WorldViewProj);
  // calc the coordinates in mirror space
  output.tex = mul(float4(input.pos, 1), MirrorViewProj);
  output.wsPos = input.pos;
  return output;
};

float4 ps_main(PsInput input) : SV_TARGET {
  
  // calc the projected texture coords
  float2 t;
  t.x = 0.5f + input.tex.x / input.tex.w / 2;
  t.y = 0.5f - input.tex.y / input.tex.w / 2;
  
  float d = saturate(1-distance(input.wsPos, float3(0,0,0)) / 100);
  return lerp(d, Texture0.Sample(LinearSampler, t), d);
};