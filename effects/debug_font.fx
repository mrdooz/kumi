// draws textures triangles in screen space

struct psInput
{
	float4	pos : SV_Position;
	float2	tex : Tex0;
};

struct vsInput
{
	float4	pos : SV_Position;
	float2	tex : Tex0;
};

psInput vsMain(in vsInput v)
{
	psInput o;
	o.pos = v.pos;
	o.tex = v.tex;
	return o;
}

float4 psMain(in psInput v) : SV_Target
{
	return float4(1,1,1,1);
}
