// draws textures triangles in screen space

void vsMain(
	in float4 i_pos : POSITION, 
	in float2 i_tex : TEXCOORD0, 
	out float4 o_pos : SV_POSITION, 
	out float2 o_tex : TEXCOORD0)
{
	o_pos = i_pos;
	o_tex = i_tex;
}

float4 psMain(in float4 pos : SV_POSITION, in float2 tex : TEXCOORD0) : SV_Target
{
	return float4(1,1,1,1);
}
