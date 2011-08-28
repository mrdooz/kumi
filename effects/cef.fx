Texture2D g_texture;
extern sampler g_sampler;

void vs_main(
	in float4 i_pos : SV_POSITION, 
	in float2 i_tex : TEXCOORD,
	out float4 o_pos : SV_POSITION, 
	out float2 o_tex : TEXCOORD)
{
	o_pos = i_pos;
	o_tex = i_tex;
}

float4 ps_main(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD) : SV_Target
{
	return g_texture.Sample(g_sampler, tex);
}
