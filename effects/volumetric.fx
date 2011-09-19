matrix proj, view, world;
float4 DiffuseColor;

struct vs_input {
	float4 pos : POSITION;
	float3 normal : NORMAL;
	float2 tex : TEXCOORD;
};

struct ps_input {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float2 tex : TEXCOORD;
};

ps_input vs_occluder(vs_input input)
{
	ps_input output = (ps_input)0;
	output.pos = mul(input.pos, world);
	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, proj);
	return output;
}

float4 ps_occluder(ps_input input) : SV_Target
{
	float c = 0.0f;
	return float4(c, c, c, 1);
}

Texture2D volumetric_occluder;
sampler shaft_sampler;

void vs_shaft(
	in float4 i_pos : SV_POSITION, 
	in float2 i_tex : TEXCOORD,
	out float4 o_pos : SV_POSITION, 
	out float2 o_tex : TEXCOORD)
{
	o_pos = i_pos;
	o_tex = i_tex;
}

float4 ps_shaft(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD) : SV_Target
{
	return volumetric_occluder.Sample(shaft_sampler, tex);
}

ps_input vs_add(vs_input input)
{
	ps_input output = (ps_input)0;
	output.pos = mul(input.pos, world);
	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, proj);
	return output;
}

float4 ps_add(ps_input input) : SV_Target
{
	float c = 0.5f;
	return float4(c, c, c, 1);
}
