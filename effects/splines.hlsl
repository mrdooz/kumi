#include "common.hlsl"

static const float3 lightPos = normalize(float3(1, 1, 1));

cbuffer test
{
  matrix worldViewProj;
  float3 cameraPos;
}

struct VsInput {
  float3 pos : POSITION;
  float3 normal : NORMAL;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float3 wsPos : POSITION;
  float3 normal : NORMAL;
};

PsInput vs_main(VsInput input)
{
    PsInput output = (PsInput)0;
    output.pos = mul(float4(input.pos, 1), worldViewProj);
    output.wsPos = input.pos;
    output.normal = input.normal;
    return output;
}

float4 ps_main(PsInput input) : SV_TARGET
{
    //return float4(1,1,0,1);
    return dot(normalize(input.normal), normalize(cameraPos - input.wsPos));
}