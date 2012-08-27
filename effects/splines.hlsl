#include "common.hlsl"

static const float3 lightPos = normalize(float3(1, 1, 1));

cbuffer test
{
  matrix worldViewProj;
  float4 cameraPos;
  float4 extrudeShape[20];
}

struct VsGsInput {
  matrix frame0 : FRAME0;
  matrix frame1 : FRAME4;
  float2 radius : RADIUS;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float3 wsPos : POSITION;
  float3 normal : NORMAL;
};

VsGsInput vs_main(VsGsInput input)
{
  return input;
}

[maxvertexcount(64)]
void gs_main(point VsGsInput input[1], inout TriangleStream<PsInput> outputStream)
{
  float r0 = input[0].radius.x;
  float r1 = input[0].radius.y;

  matrix s0 = matrix(
   r0,  0,  0,  0,
    0, r0,  0,  0,
    0,  0, r0,  0,
    0,  0,  0,  1);

  matrix s1 = matrix(
   r1,  0,  0,  0,
    0, r1,  0,  0,
    0,  0, r1,  0,
    0,  0,  0,  1);

  matrix f0 = input[0].frame0;
  matrix f1 = input[0].frame1;
  matrix mtx0 = mul(mul(s0, f0), worldViewProj);
  matrix mtx1 = mul(mul(s1, f1), worldViewProj);

  PsInput output = (PsInput)0;

  for (int i = 0; i < 21; ++i) {
  
      float3 pt = extrudeShape[i % 20].xyz;
      
      // lower level
      output.pos    = mul(float4(pt, 1), mtx0);
      output.normal = mul(pt, (float3x3)f0);
      output.wsPos  = mul(float4(pt,1), f0).xyz;
      outputStream.Append(output);

      output.pos    = mul(float4(pt, 1), mtx1);
      output.normal = mul(pt, (float3x3)f1);
      output.wsPos  = mul(float4(pt,1), f1).xyz;
      outputStream.Append(output);
  }
}

float4 ps_main(PsInput input) : SV_TARGET
{
  //return float4(input.wsPos,1);
  //return float4(1,1,0,1);
  return saturate(dot(normalize(input.normal), normalize(float3(0,0,-100) - input.wsPos)));
}