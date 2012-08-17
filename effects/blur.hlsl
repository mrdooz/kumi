#include "common.hlsl"

///////////////////////////////////
// Blurs
///////////////////////////////////

// horizontal rolling box blur, one thread per row
// reads from texture, writes to structured buffer

RWStructuredBuffer<float4> Output;

cbuffer blurSettings : register( b0 )
{
    int imageW;
    int imageH;
    float2 blurRadius;
}

[numthreads(64,1,1)]
void boxBlurX(uint3 globalThreadID : SV_DispatchThreadID)
{
    int y = globalThreadID.x;
    if (y >= imageH) 
      return;
    
    float r = blurRadius.x;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(0, y, 0));
    int i;
    for (i = 1; i <= m; ++i)
        sum += Texture0.Load(int3(-i, y, 0)) + Texture0.Load(int3(i, y, 0));
    sum += alpha * (Texture0.Load(int3(-m-1, y, 0)) + Texture0.Load(int3(m+1, y, 0)));

    for (i = 0; i < imageW; ++i) {
        Output[y*imageW+i] = sum * scale;
        sum += lerp(Texture0.Load(int3(i+m+1, y, 0)), Texture0.Load(int3(i+m+2, y, 0)), alpha);
        sum -= lerp(Texture0.Load(int3(i-m, y, 0)), Texture0.Load(int3(i-m-1, y, 0)), alpha);
    }
}

[numthreads(64,1,1)]
void boxBlurY(uint3 globalThreadID : SV_DispatchThreadID)
{
    int x = globalThreadID.x;
    if (x >= imageW)
      return;
    
    float r = blurRadius.y;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(x, 0, 0));
    int i;
    for (i = 1; i <= m; ++i)
        sum += Texture0.Load(int3(-x, i, 0)) + Texture0.Load(int3(x, i, 0));
    sum += alpha * (Texture0.Load(int3(x, -m-1, 0)) + Texture0.Load(int3(x, m+1, 0)));

    for (i = 0; i < imageH; ++i) {
        Output[i*imageW +x] = sum * scale;
        sum += lerp(Texture0.Load(int3(x, i+m+1, 0)), Texture0.Load(int3(x, i+m+2, 0)), alpha);
        sum -= lerp(Texture0.Load(int3(x, i-m, 0)), Texture0.Load(int3(x, i-m-1, 0)), alpha);
    }
}

StructuredBuffer<float4> SBuffer0 : register(t0);

// copy from structured buffer to texture
float4 ps_copy_uav(float4 pos : SV_POSITION, float2 tex : TEXCOORD0) : SV_TARGET
{
    uint2 pixelPos = floor(tex.xy * uint2(imageW, imageH));
    uint buffIndex = pixelPos.y*imageW + pixelPos.x;
    return SBuffer0[buffIndex];
}
