//
// Single color vs/ps
//
struct vs_color_input {
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
};

struct ps_color_input {
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
};

ps_color_input vs_color_main(vs_color_input input)
{
	ps_color_input output = (ps_color_input)0;
	output.pos = input.pos;
	output.col = input.col;
	return output;
}

float4 ps_color_main(ps_color_input input) : SV_Target
{
	return input.col;
}

//
// Texture mapped vs/ps
//

Texture2D diffuse_texture;
sampler diffuse_sampler;

struct vs_texture_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

struct ps_texture_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

ps_texture_input vs_texture_main(vs_texture_input input)
{
	ps_texture_input output = (ps_texture_input)0;
	output.pos = input.pos;
	output.tex = input.tex;
	return output;
}

float4 ps_texture_main(ps_texture_input input) : SV_Target
{
	return diffuse_texture.Sample(diffuse_sampler, input.tex);
}
