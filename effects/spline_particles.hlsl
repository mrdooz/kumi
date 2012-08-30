#include "common.hlsl"

///////////////////////////////////
// Spline particles
///////////////////////////////////

struct VsInput {
  float3 pos : POSITION;
  float2 scale : TEXCOORD0;
};

struct GsInput {
  float3 pos : POSITION;
  float2 scale : TEXCOORD0;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float4 tex : TEXCOORD0; // tex[u|v], radius, factor
};

cbuffer ParticleBuffer
{
  float4 cameraPos;
  matrix worldView;
  matrix proj;
}

GsInput vs_main(VsInput input)
{
  GsInput output;
  output.pos = mul(float4(input.pos,1), worldView).xyz;
  output.scale = input.scale;
  return output;
}

[maxvertexcount(4)]
void gs_main(point GsInput input[1], inout TriangleStream<PsInput> outputStream)
{
  float3 pos = input[0].pos.xyz;
  float f = input[0].scale.y;
  float r = input[0].scale.x;
  
  float3 camToParticle = pos - cameraPos.xyz;
  
  // We're in camera space right now, so our up and right vectors
  // correspond to the cardinal axis. Doh :)
  float left = pos.x - r;
  float right = pos.x + r;
  float top = pos.y + r;
  float bottom = pos.y - r;
  float z = pos.z;
  
  // 1--3
  // |  |
  // 0--2
  
  PsInput output;
  output.tex.zw = input[0].scale;
  
  output.pos = mul(float4(left, bottom, z, 1), proj);
  output.tex.xy = float2(0, 1);
  outputStream.Append(output);

  output.pos = mul(float4(left, top, z, 1), proj);
  output.tex.xy = float2(0, 0);
  outputStream.Append(output);

  output.pos = mul(float4(right, bottom, z, 1), proj);
  output.tex.xy = float2(1, 1);
  outputStream.Append(output);

  output.pos = mul(float4(right, top, z, 1), proj);
  output.tex.xy = float2(1, 0);
  outputStream.Append(output);
}

float4 ps_main(PsInput input) : SV_TARGET
{
    float4 diffuse = input.tex.w * Texture0.Sample(LinearSampler, input.tex.xy);
    return diffuse;
}
