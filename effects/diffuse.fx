matrix proj, view, world;

void vsMain(
	in float3 i_pos : POSITION, 
	in float3 i_normal : NORMAL,
	in float2 i_tex : TEXCOORD,
	out float4 o_pos : POSITION, 
	out float3 o_normal : NORMAL,
	out float2 o_tex : TEXCOORD)
{
	o_pos = mul(float4(i_pos, 1), world * view * proj);
	o_tex = i_tex;
}

float4 psMain(in float4 pos : POSITION, in float3 normal, in float2 tex : TEXCOORD) : SV_Target
{
	return float4(normal, 1);
}
