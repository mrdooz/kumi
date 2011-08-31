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

ps_input vs_main(vs_input input)
{
	ps_input output = (ps_input)0;
	output.pos = mul(input.pos, world);
	output.pos = mul(output.pos, view);
	output.pos = mul(output.pos, proj);
	output.normal = input.normal;
	output.tex = input.tex;
	return output;
}

float4 ps_main(ps_input input) : SV_Target
{
//	return float4(1,0,1,0);
	return dot(float3(1,0.5,-1), input.normal);
	return float4(input.normal, 1);
	//return float4(1,1,1,1);
	return DiffuseColor;
}
