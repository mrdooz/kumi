#include "common.hlsl"

struct VsInput {
  float3 pos : POSITION;
  float2 tex : TEXCOORD0;
  float3 tangent: TANGENT;
  float3 normal : NORMAL;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float3 wsPos : POSITION;
  float2 tex : TEXCOORD0;
  float3 tangent: TANGENT;
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
  output.tex = input.tex;
  output.tangent = input.tangent;
  output.normal = input.normal;
  return output;
}

float4 ps_main(PsInput input) : SV_TARGET
{
  float3 textureNormal = Texture0.Sample(LinearSampler, input.tex).xyz;
  textureNormal = normalize(2 * textureNormal - 1);
  
  float3 N = normalize(input.normal);
  // remove the projection of T on N to make sure T and N are still orthonormal
  float3 T = normalize(input.tangent - dot(input.tangent, N)*N);
  float3 B = cross(N, T);
  
  float3x3 TBN = float3x3(T, B, N);
  
  float3 nn = mul(textureNormal, TBN);

  float3 lightPos2 = float3(0, 100, -100);
  float3 lightDir = normalize(lightPos2 - input.wsPos);
  
  float3 eye = normalize(eyePos - input.wsPos);
  //float3 n = normalize(input.normal);
  float3 r = reflect(-eye, nn);
  float diffuse = saturate(dot(nn,eye));
  //return diffuse;
  
  float spec = pow(dot(nn,r), 200);
  return 0.2 + 0.4 * saturate(diffuse) + 0.6 * saturate(spec);
}
