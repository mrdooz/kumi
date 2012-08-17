#include "common.hlsl"

float4 ps_scale(quad_ps_input input) : SV_Target
{
    return Texture0.Sample(LinearSampler, input.tex);
}
