#include "common.hlsl"

///////////////////////////////////
// Calc luminance map
///////////////////////////////////

// Approximates luminance from an RGB value
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float4 luminance_map_ps_main(quad_ps_input input) : SV_Target
{
  float3 color = Texture0.Sample(LinearSampler, input.tex).rgb;
  
  // calculate the luminance using a weighted average
  float luminance = log(max(CalcLuminance(color), 0.00001f));
  return float4(luminance, 1.0f, 1.0f, 1.0f);
}

///////////////////////////////////
// Scaling+cutoff
///////////////////////////////////
Texture2D rt_luminance;
float4 scale_cutoff(quad_ps_input input) : SV_Target
{
  // texture1 = rt_luminance
  float3 color = Texture0.Sample(LinearSampler, input.tex).rgb;
  float avg_lum = exp(Texture1.SampleLevel(PointSampler, input.tex, 10.0));
  float cur_lum = max(CalcLuminance(color), 0.00001f);
  
  return cur_lum >= 10 * avg_lum ? float4(color, 1) : float4(0,0,0,1);
}

///////////////////////////////////
// Scaling
///////////////////////////////////
// Uses hw bilinear filtering for upscaling or downscaling
float4 scale(quad_ps_input input) : SV_Target
{
    return Texture0.Sample(LinearSampler, input.tex);
}

///////////////////////////////////
// Gaussian blur
///////////////////////////////////

// Calculates the gaussian blur weight for a given distance and sigmas
float CalcGaussianWeight(int sampleDist, float sigma)
{
  float g = 1.0f / sqrt(2.0f * 3.14159 * sigma * sigma);
  return (g * exp(-(sampleDist * sampleDist) / (2 * sigma * sigma)));
}

static const float BloomBlurSigma = 0.5;
static const float2 InputSize0 = float2(1440.0/8, 900.0/8);

// Performs a gaussian blur in one direction
float4 Blur(quad_ps_input input, float2 texScale, float sigma)
{
    float4 color = 0;
    for (int i = -6; i < 6; i++) {
      float weight = CalcGaussianWeight(i, sigma);
      float2 texCoord = input.tex;
      texCoord += (i / InputSize0) * texScale;
      float4 sample = Texture0.Sample(PointSampler, texCoord);
      color += sample * weight;
    }

    return color;
}

// Horizontal gaussian blur
float4 blur_horiz(quad_ps_input input) : SV_Target
{
    return Blur(input, float2(1, 0), BloomBlurSigma);
}

// Vertical gaussian blur
float4 blur_vert(quad_ps_input input) : SV_Target
{
    return Blur(input, float2(0, 1), BloomBlurSigma);
}

