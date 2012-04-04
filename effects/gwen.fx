Texture2D cef_texture;
sampler cef_sampler;

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
