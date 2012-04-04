cbuffer globals {
	float4 g_time;	// [total_elapsed, effect_elapsed, 0, 0]
}

struct vs_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

struct ps_input {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
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
	float r = 0, g = 0, b = 0;
	for (int i = 0; i < 10; i++) {
		float x = input.tex.x * g_time[0];
		float y = input.tex.y + sin(x/100*g_time[1]*x/100);
		r += (0.5 + 0.7*(x-0.5)*sin(20*(y-0.5))) / 10;
		g += (0.2 + 0.7*(r-0.5)*sin(2*(y-0.5))) / 1;
		b += (0.1 + 0.7*(x-0.2)*sin(5*(y-0.5))) / 10;
	}
	return float4(r, g, b, 0);
}

