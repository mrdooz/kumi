#include "common.hlsl"

struct VsInput {
  float3 pos : POSITION;
  float3 normal : NORMAL;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float3 wsPos : POSITION;
  float3 normal : NORMAL;
};

cbuffer cbMain {
  matrix worldViewProj;
  matrix worldView;
  float3 lightPos;
  float3 eyePos;
};

PsInput vs_main(VsInput input)
{
  PsInput output;
  output.pos = mul(float4(input.pos, 1), worldViewProj);
  output.wsPos = input.pos;
  output.normal = input.normal;
  return output;
}

float4 ps_main(PsInput input) : SV_TARGET
{
  float3 eye = normalize(eyePos - input.wsPos);
  float3 n = normalize(input.normal);
  float3 r = reflect(-eye, n);
  float diffuse = dot(n,eye);
  float spec = pow(dot(n,r), 100);
  return 0.2 + 0.5 * saturate(diffuse) + 0.7 * saturate(spec);
  return saturate( pow(dot(n,r), 20));

  return 0.3 + 0.7 * saturate(dot(normalize(input.normal), normalize(lightPos - input.wsPos)));
}
