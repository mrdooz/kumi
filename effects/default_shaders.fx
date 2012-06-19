matrix proj, view, world;
float4 LightColor, LightPos;

cbuffer TestCb {
    float4 Diffuse;
};

///////////////////////////////////
// diffuse
///////////////////////////////////
struct diffuse_vs_input {
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

struct diffuse_ps_input {
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
    float4 pos_ws : TEXCOORD1;
};

diffuse_ps_input diffuse_vs_main(diffuse_vs_input input)
{
    diffuse_ps_input output = (diffuse_ps_input)0;
    output.pos_ws = mul(input.pos, world);
    output.pos = mul(output.pos_ws, view);
    output.pos = mul(output.pos, proj);
    output.normal = mul(float4(input.normal, 0), world);
    output.tex = input.tex;
    return output;
}

float4 diffuse_ps_main(diffuse_ps_input input) : SV_Target
{
    float3 dir = float3(LightPos.x - input.pos_ws.x, LightPos.y - input.pos_ws.y, LightPos.z - input.pos_ws.z);
    dir = normalize(dir);
    return 0.5 + dot(dir, normalize(input.normal)) * Diffuse;
}

///////////////////////////////////
// texture
///////////////////////////////////
Texture2D diffuse_texture : register(t0);
sampler diffuse_sampler : register(s0);

struct texture_vs_input {
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

struct texture_ps_input {
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
    float4 pos_ws : TEXCOORD1;
};

texture_ps_input texture_vs_main(texture_vs_input input)
{
    texture_ps_input output = (texture_ps_input)0;
    output.pos_ws = mul(input.pos, world);
    output.pos = mul(output.pos_ws, view);
    output.pos = mul(output.pos, proj);
    output.normal = mul(float4(input.normal, 0), world);
    output.tex = input.tex;
    return output;
}

float4 texture_ps_main(texture_ps_input input) : SV_Target
{
    float4 diffuse = diffuse_texture.Sample(diffuse_sampler, input.tex);

    float3 dir = float3(LightPos.x - input.pos_ws.x, LightPos.y - input.pos_ws.y, LightPos.z - input.pos_ws.z);
    dir = normalize(dir);
    //return dot(dir, normalize(input.normal)) * (diffuse + Diffuse);
    return dot(dir, normalize(input.normal)) * (diffuse);
}
