#include "common.hlsl"

cbuffer ParticleBuffer
{
  float4 cameraPos;
  matrix worldView;
  matrix proj;
  float4 DOFDepths;
}

// Computes the depth of field blur factor
float BlurFactor(in float depth) {
    float f0 = 1.0f - saturate((depth - DOFDepths.x) / max(DOFDepths.y - DOFDepths.x, 0.01f));
    float f1 = saturate((depth - DOFDepths.z) / max(DOFDepths.w - DOFDepths.z, 0.01f));
    float blur = saturate(f0 + f1);
    return blur;
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
  float3 scale : TEXCOORD0;   // radius, factor, blur
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
  output.scale.xy = input[0].scale;
  output.scale.z = BlurFactor(camToParticle.z);
  
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

struct ps_output {
    float4 diffuse : SV_TARGET0;
    float depth : SV_TARGET1;
};

ps_output ps_main(PsInput input)
{
    ps_output output = (ps_output)0;
    output.diffuse = 0.1 * input.scale.y * Texture0.Sample(LinearSampler, input.tex);
    output.depth = input.scale.z;
    return output;
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

///////////////////////////////////
// Coalesce diffuse, cut-off diffuse and depth
///////////////////////////////////
float4 ps_coalesce(quad_ps_input input) : SV_Target
{
    float diffuse = Texture0.Sample(LinearSampler, input.tex).r;
    float depth = Texture1.Sample(LinearSampler, input.tex).r;
    return float4(diffuse, diffuse > 0.5f ? 3 * diffuse : 0, depth, 0);
}

///////////////////////////////////
// Compose
///////////////////////////////////
float4 ps_compose(quad_ps_input input) : SV_Target
{
    // Texture0 = [sample, bloom-sample, dof-value]
    float4 background = Texture2.Sample(LinearSampler, input.tex);
    float4 a = Texture0.Sample(LinearSampler, input.tex);
    float4 b = Texture1.Sample(LinearSampler, input.tex);
    float t = b.z;
    return background + lerp(b.x, a.x, t) + b.y;
}
