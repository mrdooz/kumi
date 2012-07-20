#include "common.fx"

cbuffer PerFrame {
  matrix proj;
  matrix view;
  float4 kernel[32];
  float4 noise[16];
};
/*
cbuffer PerFrame {
  float4 kernel[32];
  float4 noise[16];
};
*/
cbuffer PerMaterial {
  float4 Diffuse;
  float4 Specular;
  float Shininess;
};

cbuffer PerMesh {
  matrix world;
};

static const float ScreenWidth = 1440;
static const float ScreenHeight = 900;

static const float Gamma = 2.0;
static const float InvGamma = 1/Gamma;

#if DIFFUSE_TEXTURE
Texture2D DiffuseTexture : register(t0);
#endif

///////////////////////////////////
// g-buffer fill
///////////////////////////////////
struct fill_vs_input {
    float4 pos : POSITION;
    float3 normal : NORMAL;
#if DIFFUSE_TEXTURE
    float2 tex : TEXCOORD;
#endif
};

struct fill_ps_input {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 vs_pos : TEXCOORD1;
    float4 vs_normal : TEXCOORD2;
};

struct fill_ps_output {
    float4 rt_pos : SV_TARGET0;
    float4 rt_normal : SV_TARGET1;
    float4 rt_diffuse : SV_TARGET2;
    float4 rt_specular : SV_TARGET3;
};

fill_ps_input fill_vs_main(fill_vs_input input)
{
    fill_ps_input output = (fill_ps_input)0;
    float4x4 world_view = mul(world, view);
    output.pos = mul(input.pos, mul(world_view, proj));
#if DIFFUSE_TEXTURE
    output.tex = input.tex;
#endif
    output.vs_pos = mul(input.pos, world_view);
    output.vs_normal = mul(float4(input.normal,0), world_view);
    return output;
}

fill_ps_output fill_ps_main(fill_ps_input input)
{
    fill_ps_output output = (fill_ps_output)0;
    output.rt_pos = input.vs_pos;
    output.rt_normal = normalize(input.vs_normal);
  #if DIFFUSE_TEXTURE
    // convert to linear space
    output.rt_diffuse = pow(DiffuseTexture.Sample(LinearSampler, input.tex), Gamma);
  #else
    output.rt_diffuse = Diffuse;
  #endif
    output.rt_specular.rgb = Specular.rgb;
    output.rt_specular.a = Shininess;
    return output;
}

///////////////////////////////////
// Occlusion compute
///////////////////////////////////

Texture2D rt_pos : register(t0);
Texture2D rt_normal : register(t1);
Texture2D rt_diffuse : register(t2);
Texture2D rt_specular : register(t3);


float compute_ps_main(quad_ps_input input) : SV_Target
{
    float3 origin = rt_pos.Sample(PointSampler, input.tex).xyz;
    float3 normal = rt_normal.Sample(PointSampler, input.tex).xyz;
   
    // tile the noise in a 4x4 grid
    int x = (int)(ScreenWidth * input.tex.x);
    int y = (int)(ScreenHeight * input.tex.y);
    int idx = (y%4)*4 + (x%4);
    float3 rvec = noise[idx].xyz;
    
    // Gram-Schmidt
    float3 tangent = normalize(rvec - normal * dot(rvec, normal));
    float3 bitangent = cross(normal, tangent);
    
    float3x3 tbn = float3x3( 
        tangent.x, tangent.y, tangent.z,
        bitangent.x, bitangent.y, bitangent.z,
        normal.x, normal.y, normal.z);
        
    float occlusion = 0.0;
    float radius = 27;
    int KERNEL_SIZE = 32;
    for (int i = 0; i < KERNEL_SIZE; ++i) {
        // get sample position
        float3 sample = mul(kernel[i].xyz, tbn);
        sample = sample * radius + origin;
        
        // project sample position
        float4 offset = float4(sample, 1.0);
        offset = mul(offset, proj); // to clip space
        offset.xy /= offset.w; // to NDC
        offset.xy = offset.xy * 0.5 + 0.5; // to texture coords
        offset.y = 1 - offset.y;
        
        float sample_depth = rt_pos.Sample(PointSampler, offset.xy).z;

        // check for big discontinuities in z
        float range_check = abs(origin.z - sample_depth) < radius ? 1.0 : 0.0;
        occlusion += (sample_depth <= sample.z ? 1.0 : 0.0) * range_check;
    }
    
    return origin.z > 0 ? 1.0 - (occlusion / KERNEL_SIZE) : 0;
}

///////////////////////////////////
// blur
///////////////////////////////////

float blur_ps_main(quad_ps_input input) : SV_Target
{
    float res = 0.0;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float2 ofs = float2(1/ScreenWidth * j, 1/ScreenHeight*i);
            res += Texture0.Sample(PointSampler, input.tex + ofs).r;
        }
    }
    float occ = res / 16.0;
    return occ;
}

///////////////////////////////////
// Render ambient * occlusion
///////////////////////////////////

Texture2D rt_occlusion;

cbuffer PerFrame {
  float4 Ambient;
};

float4 ambient_ps_main(quad_ps_input input) : SV_Target
{
  return Ambient * rt_occlusion.Sample(PointSampler, input.tex).r;
}

///////////////////////////////////
// Add light
///////////////////////////////////
cbuffer PerInstance {
  float4 LightColor, LightPos;
  float AttenuationStart, AttenuationEnd;
};

float4 light_ps_main(quad_ps_input input) : SV_Target
{
    float3 pos = rt_pos.Sample(PointSampler, input.tex).xyz;
    float3 normal = normalize(rt_normal.Sample(PointSampler, input.tex).xyz);
    float4 diffuse = rt_diffuse.Sample(PointSampler, input.tex);
    float4 st = rt_specular.Sample(PointSampler, input.tex);
    float3 specular = st.rgb;
    float shininess = st.a;
    float3 lp = LightPos.xyz;
    float3 ll = lp - pos;
    float3 v = normalize(ll);
    float dist = length(ll);
    
    float4 dd;
    float scale;

    if (dist < AttenuationStart) {
      dd = saturate(dot(v, normal)) * diffuse * LightColor;
      scale = 1;
    } else if (dist > AttenuationEnd) {
      dd = float4(0,0,0,0);
      scale = 0;
    } else {
      scale = 1 - lerp(0, 1, (dist - AttenuationStart) / (AttenuationEnd - AttenuationStart));
      dd = saturate(dot(v, normal)) * diffuse * LightColor;
    }
    
    float3 h = normalize(-pos + ll);
    float ss = specular * pow(saturate(dot(h, normal)), shininess);

    float occ = rt_occlusion.Sample(PointSampler, input.tex).r;
    
    float decay = dist < 40 ? 1 : 40 / dist;
   
    return pow(occ, 2)  * decay * scale * (dd + ss);
}


///////////////////////////////////
// Gamma correction
///////////////////////////////////
Texture2D rt_composite : register(t0);

float4 gamma_ps_main(quad_ps_input input) : SV_Target
{
  float4 color = rt_composite.Sample(PointSampler, input.tex);
  float4 bloom = Texture1.Sample(LinearSampler, input.tex);
  return pow(color + 0.25 * bloom, InvGamma);
}
