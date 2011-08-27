Texture2D g_texture, g_texture2;
sampler g_sampler, g_sampler2;

float4 tjong_value;
cbuffer buf1 {
float4 tjong_value_2;
};

cbuffer buf2 {
float4 t, t2, t3, t4, t5, t6;
};

void vs_main(
	in float4 i_pos : SV_POSITION, 
	in float2 i_tex : TEXCOORD,
	out float4 o_pos : SV_POSITION, 
	out float2 o_tex : TEXCOORD)
{
	o_pos = i_pos;
	o_tex = i_tex + tjong_value_2.xy;
}

float4 ps_main(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD) : SV_Target
{
	return 
	g_texture.Sample(g_sampler, tex) + 
	g_texture2.Sample(g_sampler2, tex) + 
	tjong_value + tjong_value_2 + t + t2 + t3 + t4 + t5 + t6;
}

