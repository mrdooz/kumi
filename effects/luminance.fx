struct fullscreen_vs_input {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

struct fullscreen_ps_input {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

fullscreen_ps_input quad_vs_main(fullscreen_vs_input input)
{
    fullscreen_ps_input output = (fullscreen_ps_input)0;
    output.pos = input.pos;
    output.tex = input.tex;
    return output;
}

///////////////////////////////////
// Calc luminance map
///////////////////////////////////

Texture2D rt_composite : register(t0);
sampler LinearSampler : register(s0);

// Approximates luminance from an RGB value
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float4 luminance_map_ps_main(fullscreen_ps_input input) : SV_Target
{
  float3 color = rt_composite.Sample(LinearSampler, input.tex).rgb;
  
  // calculate the luminance using a weighted average
  float luminance = log(max(CalcLuminance(color), 0.00001f));
  return float4(luminance, 1.0f, 1.0f, 1.0f);
}

/*
///////////////////////////////////
// Remove pixels below the luminance threshold
///////////////////////////////////
float4 threshold_ps_main(fullscreen_ps_input input) : SV_Target
{
  float avg_lum = exp(rt_luminance.SampleLevel(ssao_sampler, input.tex, 10.0));
  float4 color = pow(rt_composite.Sample(ssao_sampler, input.tex), InvGamma);
  float pixel_lum = max(CalcLuminance(color), 0.00001f);
  
  return pixel_lum < avg_lum ? float4(0,0,0,1) : float4(color.r - avg_lum, color.g - avg_lum, color.b - avg_lum, 1);
}


float4 scale_ps_main(fullscreen_ps_input input) : SV_Target
{
  float avg_lum = exp(rt_luminance.SampleLevel(ssao_sampler, input.tex, 10.0));
  float4 color = pow(rt_composite.Sample(ssao_sampler, input.tex), InvGamma);
  float pixel_lum = max(CalcLuminance(color), 0.00001f);
  
  return pixel_lum < avg_lum ? float4(0,0,0,1) : float4(color.r - avg_lum, color.g - avg_lum, color.b - avg_lum, 1);
}
*/