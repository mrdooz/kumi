#include "common.hlsl"

cbuffer ParticleBuffer
{
  float4  wsCameraPos;
  matrix worldView;
  matrix proj;
}

struct VsInput {
  float4 pos : POSITION;
  float radius : SIZE;
};

struct GsInput {
  float4 pos : POSITION;
  float radius : SIZE;
};

struct PsInput {
  float4 pos : SV_Position;
  float2 tex : TEXCOORD0;
};

GsInput vs_main(VsInput input)
{
  GsInput output = (GsInput)0;
  output.pos = mul(input.pos, worldView);
  output.radius = input.radius;
  return output;
}

PsInput gs_main(point GsInput input[1], inout TriangleStream<PsInput> outputStream)
{
  float3 pos = input[0].pos.xyz;
  float r = input[0].radius;
  float3 camToParticle = pos - wsCameraPos.xyz;
  float3 rightVec = normalize(cross(float3(0,1,0), camToParticle));
  
  float3 left = pos - r * rightVec;
  float3 right = pos + r * rightVec;
  
  // 1--3
  // |  |
  // 0--2
  
  PsInput output = (PsInput)0;
  output.pos = mul(float4(left.x, left.y - r, left.z, 1), proj);
  output.tex = float2(0, 1);
  outputStream.Append(output);

  output.pos = mul(float4(left.x, left.y + r, left.z, 1), proj);
  output.tex = float2(0, 0);
  outputStream.Append(output);

  output.pos = mul(float4(left.x + r, left.y - r, left.z, 1), proj);
  output.tex = float2(1, 1);
  outputStream.Append(output);

  output.pos = mul(float4(left.x + r, left.y + r, left.z, 1), proj);
  output.tex = float2(1, 0);
  outputStream.Append(output);
}

float4 ps_main(PsInput input) : SV_Target
{
  return Texture0.Sample(LinearSampler, input.tex);
}
