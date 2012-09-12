#include "common.hlsl"

struct VsInput {
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
};

struct PsInput {
  float4 pos : SV_POSITION;
  float2 tex : TEXCOORD0;
};

PsInput vs_main(VsInput input)
{
  PsInput output;
  output.pos = input.pos;
  output.tex = input.tex;
  return output;
}

float4 ps_main(PsInput input) : SV_TARGET
{
  float4 col = Texture0.Sample(LinearSampler, input.tex);
  return col;
  return input.pos.x / 1000;
}
