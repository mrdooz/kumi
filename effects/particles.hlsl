#include "common.hlsl"

cbuffer ParticleBuffer
{
  float4 cameraPos;
  matrix worldView;
  matrix proj;
}

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
  float2 scale : TEXCOORD0;
  float2 tex : TEXCOORD1;
};

GsInput vs_main(VsInput input)
{
  GsInput output = (GsInput)0;
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
  float3 rightVec = normalize(cross(float3(0,1,0), camToParticle));
  
  float left = (pos - r * rightVec).x;
  float right = (pos + r * rightVec).x;
  float top = pos.y + r;
  float bottom = pos.y - r;
  float z = pos.z;
  
  // 1--3
  // |  |
  // 0--2
  
  PsInput output = (PsInput)0;
  output.scale = input[0].scale;
  
  output.pos = mul(float4(left, bottom, z, 1), proj);
  output.tex = float2(0, 1);
  outputStream.Append(output);

  output.pos = mul(float4(left, top, z, 1), proj);
  output.tex = float2(0, 0);
  outputStream.Append(output);

  output.pos = mul(float4(right, bottom, z, 1), proj);
  output.tex = float2(1, 1);
  outputStream.Append(output);

  output.pos = mul(float4(right, top, z, 1), proj);
  output.tex = float2(1, 0);
  outputStream.Append(output);
}

float4 ps_main(PsInput input) : SV_Target
{
  return 0.1 * input.scale.y * Texture0.Sample(LinearSampler, input.tex);
}

///////////////////////////////////
// Gradient
///////////////////////////////////
float4 gradient_ps_main(quad_simple_ps_input input) : SV_Target
{
    float width = 1440;
    float x = input.pos.x/width;
    float fadeInStart = 0.1;
    float fadeInEnd = 0.475;
    float fadeOutStart = 1 - fadeInEnd;
    float fadeOutEnd = 1 - fadeInStart;

    float a = 1 - saturate((x - fadeInStart) / (fadeInEnd - fadeInStart));
    float b = saturate((x - fadeOutStart) / (fadeOutEnd - fadeOutStart));
    float c = 1 - saturate(a+b);

    float4 outer = float4(25.f/255, 15.f/255, 37.f/255, 255.f/255);
    float4 inner = float4(41.f/255, 78.f/255, 124.f/255, 255.f/255);

    return lerp(outer, inner, c);
}
