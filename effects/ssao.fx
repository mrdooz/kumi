matrix proj, view, world;
float4 Diffuse, LightColor, LightPos;

///////////////////////////////////
// g-buffer fill
///////////////////////////////////
struct fill_vs_input {
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

struct fill_ps_input {
    float4 pos : SV_POSITION;
    float4 vs_pos : TEXCOORD0;
    float4 vs_normal : TEXCOORD1;
};

struct fill_ps_output {
    float4 rt0 : COLOR1;
    float4 rt1 : COLOR2;
};

fill_ps_input fill_vs_main(fill_vs_input input)
{
    fill_ps_input output = (fill_ps_input)0;
    float4x4 world_view = mul(world, view);
    output.pos = mul(input.pos, mul(world_view, proj));
    output.vs_pos = mul(input.pos, world_view);
    output.vs_normal = mul(float4(input.normal,0), world_view);
    return output;
}

fill_ps_output fill_ps_main(fill_ps_input input) : SV_Target
{
    fill_ps_output output = (fill_ps_output)0;
    output.rt0 = input.vs_pos;
    output.rt1 = input.vs_normal;
    return output;
}

///////////////////////////////////
// actual render
///////////////////////////////////
