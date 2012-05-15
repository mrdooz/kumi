matrix proj, view, world;
float4 Diffuse, LightColor, LightPos;

struct vs_input {
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

struct ps_input {
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD0;
    float4 pos_ws : TEXCOORD1;
};

ps_input vs_main(vs_input input)
{
    ps_input output = (ps_input)0;
    output.pos_ws = mul(input.pos, world);
    output.pos = mul(output.pos_ws, view);
    output.pos = mul(output.pos, proj);
    output.normal = mul(float4(input.normal, 0), world);
    output.tex = input.tex;
    return output;
}

float4 ps_main(ps_input input) : SV_Target
{
    float3 dir = float3(LightPos.x - input.pos_ws.x, LightPos.y - input.pos_ws.y, LightPos.z - input.pos_ws.z);
    dir = normalize(dir);
    return dot(dir, normalize(input.normal)) * Diffuse;
}

