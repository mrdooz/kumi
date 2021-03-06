Texture2D bloom_rt : register(t0);
sampler diffuse_sampler : register(s0);

cbuffer globals {
	float4 g_time;	// [total_elapsed, effect_elapsed, 0, 0]
	float4 g_screen_size;
}

struct vs_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

struct ps_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

ps_input vs_main(vs_input input)
{
	ps_input output = (ps_input)0;
	output.pos = input.pos;
	output.tex = input.tex;
	return output;
}

float4 ps_main(ps_input input) : SV_Target
{
	//return bloom_rt.Sample(diffuse_sampler, input.tex);

	float4 bloom = float4(0,0,0,0);
	float span = 10;
	//float span = 6 + 5 * sin(5*g_time.x);
	int tt = abs(span);
	for (int i = -tt; i <= tt; ++i) {
		float ofs = i;
		bloom += bloom_rt.Sample(diffuse_sampler, input.tex + float2(0,ofs/100)) / (2*tt+1);
	}

	//return bloom;
	return bloom_rt.Sample(diffuse_sampler, input.tex) + bloom;
}
