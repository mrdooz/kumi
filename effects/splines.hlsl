#include "common.hlsl"

static const float3 lightPos = normalize(float3(1, 1, 1));

cbuffer test
{
  matrix worldViewProj;
  float4 cameraPos;
  float4 extrudeShape[21];
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

  matrix f0 = mul(s0, input[0].frame0);
  matrix f1 = mul(s1, input[0].frame1);
  matrix mtx0 = mul(f0, worldViewProj);
  matrix mtx1 = mul(f1, worldViewProj);

  PsInput output = (PsInput)0;
  
  float3 center0 = float3(f0[3][0], f0[3][1], f0[3][2]);
  float3 center1 = float3(f1[3][0], f1[3][1], f1[3][2]);

  for (int i = 0; i < 21; ++i) {
  
      float3 pt3 = extrudeShape[i].xyz;
      float4 pt4 = extrudeShape[i];
      
      // lower level
      output.pos    = mul(pt4, mtx0);
      output.wsPos  = mul(pt4, f0).xyz;
      output.normal = output.wsPos.xyz - center0;
      outputStream.Append(output);

      // upper
      output.pos    = mul(pt4, mtx1);
      output.wsPos  = mul(pt4, f1).xyz;
      output.normal = output.wsPos.xyz - center1;
      outputStream.Append(output);
  }
}

struct PsOutput {
  float4 color : SV_TARGET0;
  float4 occlude : SV_TARGET1;
};

float grayscale(float4 color) {
  return 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
}

PsOutput ps_main(PsInput input)
{
    PsOutput output;

    float3 dir = normalize(cameraPos.xyz - input.wsPos);
    float3 n = normalize(input.normal);
    float3 r = reflect(-dir, n);
    float specular = pow(saturate(dot(dir, r)), 100);
    float4 spec4 = float4(specular, specular, specular, specular);
    float diffuse = saturate(dot(n, normalize(float3(0,10000,-1000) - input.wsPos)));
    float t = saturate(1-0.7f*dot(n, dir));
    
    output.color = 0.5f * diffuse + lerp(TextureCube0.Sample(LinearSamplerWrap, r), spec4, t);
    output.occlude = grayscale(output.color) > 0.7 ? output.color : 0;
    return output;
}

///////////////////////////////////
// Gradient
///////////////////////////////////

cbuffer WorldToScreenSpace {
  float4 screenSpaceLight;
  float4 textureSpaceLight;
  float2 screenSize;
}

float4 gradient_ps_main(quad_simple_ps_input input) : SV_Target
{
    // hehe, this realy is the ugliest thing evar :)
    return 0;
    float2 d = input.pos.xy - screenSpaceLight.xy;
    d.x /= screenSize.x;
    d.y /= screenSize.y;
    return 1 - saturate(2*sqrt(d.x*d.x + d.y*d.y));
    
    float width = 1440;
    float height = 1000;
    float x = input.pos.x/width;
    float y = input.pos.y/height;
    float fadeInStart = 0.1;
    float fadeInEnd = 0.475;
    float fadeOutStart = 1 - fadeInEnd;
    float fadeOutEnd = 1 - fadeInStart;

    float dx = x*y;
    
    float a = 1 - saturate((dx - fadeInStart) / (fadeInEnd - fadeInStart));
    float b = saturate((dx - fadeOutStart) / (fadeOutEnd - fadeOutStart));
    float c = 1 - saturate(a+b);

    float4 outer = float4(25.f/255, 15.f/255, 37.f/255, 255.f/255);
    float4 inner = float4(41.f/255, 78.f/255, 124.f/255, 255.f/255);

    return lerp(outer, inner, c);
}

///////////////////////////////////
// Compose
///////////////////////////////////

float4 ps_compose(quad_ps_input input) : SV_Target
{
    float4 base = Texture0.Sample(LinearSampler, input.tex);
    float4 blur = Texture1.Sample(LinearSampler, input.tex);
    return base + 2.5 * blur;

    float Density = 1.8f;
    float Weight = 0.9f;
    float Decay = 0.5f;
    float Exposition = 0.5f;

    int numSteps = 30;
    float2 v = (input.tex - textureSpaceLight.xy) / (numSteps * Density);

    float illuminationDecay = 1.0f;
    float4 color = Texture1.Sample(LinearSampler, input.tex);
    float2 uv = input.tex;
    float4 bonus = float4(0,0,0,0);
    for (int i = 0; i < numSteps; ++i) {
        uv -= v;
        float4 tmp = Texture1.Sample(LinearSampler, uv);
        tmp *= illuminationDecay * Weight;
        color += tmp;
        illuminationDecay *= Decay;
    }

    return color * Exposition + base;
}

//http://http.developer.nvidia.com/GPUGems3/gpugems3_ch13.html
float4 ps_volumetric_compose(quad_ps_input input) : SV_Target
{
    float4 base = Texture0.Sample(LinearSampler, input.tex);

    float Density = 1.8f;
    float Weight = 0.9f;
    float Decay = 0.5f;
    float Exposition = 0.5f;

    int numSteps = 30;
    float2 v = (input.tex - textureSpaceLight.xy) / (numSteps * Density);

    float illuminationDecay = 1.0f;
    float4 color = Texture1.Sample(LinearSampler, input.tex);
    float2 uv = input.tex;
    float4 bonus = float4(0,0,0,0);
    for (int i = 0; i < numSteps; ++i) {
        uv -= v;
        float4 tmp = Texture1.Sample(LinearSampler, uv);
        tmp *= illuminationDecay * Weight;
        color += tmp;
        illuminationDecay *= Decay;
    }

    return color * Exposition + base;
}
