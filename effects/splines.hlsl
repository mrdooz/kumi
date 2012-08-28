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

float4 ps_main(PsInput input) : SV_TARGET
{
    float3 dir = normalize(cameraPos.xyz - input.wsPos);
    float3 n = normalize(input.normal);
    float3 r = reflect(-dir, n);
//    return pow(dot(dir, r), 100);
    float specular = pow(saturate(dot(dir, r)), 100);
    float4 spec4 = float4(specular, specular, specular, specular);
    float diffuse = saturate(dot(n, normalize(float3(0,10000,-1000) - input.wsPos)));
    float t = saturate(1-0.7f*dot(n, dir));
    return 0.5f * diffuse + lerp(TextureCube0.Sample(LinearSamplerWrap, r), spec4, t);
    
    return saturate(1-dot(n, dir)) * Texture0.Sample(LinearSampler, r);
  //return 0.3f + saturate(dot(normalize(input.normal), normalize(float3(0,10000,-1000) - input.wsPos)));
}
