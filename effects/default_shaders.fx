matrix proj, view, world;
float4 Diffuse, LightColor, LightPos;

#if DIFFUSE_TEXTURE
Texture2D diffuse_texture : register(t0);
sampler diffuse_sampler : register(s0);
#endif

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
#if DIFFUSE_TEXTURE
    return 0.5 + dot(dir, normalize(input.normal)) * diffuse_texture.Sample(diffuse_sampler, input.tex);
#else
    return 0.5 + dot(dir, normalize(input.normal)) * Diffuse;
#endif
}
