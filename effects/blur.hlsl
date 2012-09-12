#include "common.hlsl"

///////////////////////////////////
// Blurs
///////////////////////////////////

// horizontal rolling box blur, one thread per row
// reads from texture, writes to structured buffer

RWTexture2D<float4> Output : register(u0);

cbuffer blurSettings : register( b0 )
{
    float blurRadius;
}

[numthreads(64,1,1)]
void boxBlurX(uint3 globalThreadID : SV_DispatchThreadID)
{
    int dstWidth, dstHeight;
    Output.GetDimensions(dstWidth, dstHeight);

    int y = globalThreadID.x;
    if (y >= dstHeight) 
      return;

    int i;
    
    for (i = 0; i < dstWidth; ++i) {
      Output[int2(i, y)] = Texture0.Load(int3(i,y,0));
    }
    //return;
    
    float r = 1;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(0, y, 0));
    for (i = 1; i <= m; ++i)
        sum += Texture0.Load(int3(-i, y, 0)) + Texture0.Load(int3(i, y, 0));
    sum += alpha * (Texture0.Load(int3(-m-1, y, 0)) + Texture0.Load(int3(m+1, y, 0)));

    for (i = 0; i < dstWidth; ++i) {
        Output[int2(i,y)] = sum * scale;
        sum += lerp(Texture0.Load(int3(i+m+1, y, 0)), Texture0.Load(int3(i+m+2, y, 0)), alpha);
        sum -= lerp(Texture0.Load(int3(i-m, y, 0)), Texture0.Load(int3(i-m-1, y, 0)), alpha);
    }
}

[numthreads(64,1,1)]
void boxBlurY(uint3 globalThreadID : SV_DispatchThreadID)
{
    int dstWidth, dstHeight;
    Output.GetDimensions(dstWidth, dstHeight);

    int x = globalThreadID.x;
    if (x >= dstWidth)
      return;

    int i;
    
    for (i = 0; i < dstHeight; ++i) {
      Output[int2(x, i)] = Texture0.Load(int3(x, i, 0));
      //Output[int2(x, i)] = float4(1,1,0,1); //Texture0.Load(int3(x, i, 0));
    }
    //return;

    
    float r = 1;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(x, 0, 0));
    for (i = 1; i <= m; ++i)
        sum += Texture0.Load(int3(x, -i, 0)) + Texture0.Load(int3(x, i, 0));
    sum += alpha * (Texture0.Load(int3(x, -m-1, 0)) + Texture0.Load(int3(x, m+1, 0)));

    float4 tmp[2048];
    for (i = 0; i < dstHeight; ++i) {
        //Output[int2(x,i)] = 0.5 * sum * scale;
        tmp[i] = sum * scale;
        sum += lerp(Texture0.Load(int3(x, i+m+1, 0)), Texture0.Load(int3(x, i+m+2, 0)), alpha);
        sum -= lerp(Texture0.Load(int3(x, i-m, 0)), Texture0.Load(int3(x, i-m-1, 0)), alpha);
    }
    
    for (i = 0; i < dstHeight; ++i) {
        Output[int2(x,i)] = min(float4(1,1,1,1), tmp[i]);
    }

}
