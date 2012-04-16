cbuffer globals {
	float4 g_time;	// [total_elapsed, effect_elapsed, 0, 0]
	float4 g_screen_size;
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
	// determine correct y at current x
	float nx = input.pos.x / g_screen_size.x;
	float ny = input.pos.y / g_screen_size.y;
	float r = 0, g = 0, b = 0;
	
	float freq[10] = { 2, 3, 4, 5, 10, 7, 6, 4, 2, 1 };
	float amp[10] = { 2, 3, 4, 0.5, 1, 0.7, 6, 4, 2, 1 };
	
	for (int i = 0; i < 10; ++i) {
		float y = 0.5 + clamp(amp[i]*0.5*sin(freq[i]*3.1415 * nx + ((i+1)*(i+g_time[0].x))/5), 0, 1);
		float dd = clamp(pow(abs(ny-y), g_time.x/5), 0.1, 100);
		float d = 0.08/dd; //clamp(1.0 - abs(ny-y), 0, 1);
		b += d / 10;
	}
	return float4(pow(b,2.1),pow(b,6),b,b);
}

