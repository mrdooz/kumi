#include "common.hlsl"

struct VsInput {
  float3 pos : POSITION;
};

struct PsInput {
  float4 pos : SV_POSITION;
};

cbuffer cbMain {
  matrix worldViewProj;
};

PsInput vs_main(VsInput input)
{
  PsInput output;
  output.pos = mul(float4(input.pos, 1), worldViewProj);
  return output;
}

float4 ps_main(PsInput intput) : SV_TARGET
{
  return float4(1,1,0,1);
}