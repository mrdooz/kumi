Texture2D Texture0 : register(t0);
Texture2D Texture1 : register(t1);
Texture2D Texture2 : register(t2);
Texture2D Texture3 : register(t3);

TextureCube TextureCube0 : register(t0);

sampler LinearSampler;
sampler PointSampler;
sampler LinearSamplerWrap;

struct quad_vs_input {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

struct quad_ps_input {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

quad_ps_input quad_vs_main(quad_vs_input input)
{
    quad_ps_input output = (quad_ps_input)0;
    output.pos = input.pos;
    output.tex = input.tex;
    return output;
}

struct quad_simple_vs_input {
    float4 pos : SV_POSITION;
};

struct quad_simple_ps_input {
    float4 pos : SV_POSITION;
};

quad_simple_ps_input quad_simple_vs_main(quad_simple_vs_input input)
{
    quad_simple_ps_input output = (quad_simple_ps_input)0;
    output.pos = input.pos;
    return output;
}
