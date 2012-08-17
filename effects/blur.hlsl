#include "common.hlsl"

///////////////////////////////////
// Blurs
///////////////////////////////////

// horizontal rolling box blur, one thread per row
// reads from texture, writes to structured buffer

RWTexture2D<float4> Output;

cbuffer blurSettings : register( b0 )
{
    int dstWidth, dstHeight;
    float blurRadius;
}

[numthreads(64,1,1)]
void boxBlurX(uint3 globalThreadID : SV_DispatchThreadID)
{
    int y = globalThreadID.x;
    if (y >= dstHeight) 
      return;
    
    float r = blurRadius;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(0, y, 0));
    int i;
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
    int x = globalThreadID.x;
    if (x >= dstWidth)
      return;
    
    float r = blurRadius;
    float scale = 1.0f / (2*r+1);
    int m = (int)r;      // integer part of radius
    float alpha = r - m;   // fractional part

    float4 sum = Texture0.Load(int3(x, 0, 0));
    int i;
    for (i = 1; i <= m; ++i)
        sum += Texture0.Load(int3(-x, i, 0)) + Texture0.Load(int3(x, i, 0));
    sum += alpha * (Texture0.Load(int3(x, -m-1, 0)) + Texture0.Load(int3(x, m+1, 0)));

    for (i = 0; i < dstHeight; ++i) {
        Output[int2(x,i)] = sum * scale;
        sum += lerp(Texture0.Load(int3(x, i+m+1, 0)), Texture0.Load(int3(x, i+m+2, 0)), alpha);
        sum -= lerp(Texture0.Load(int3(x, i-m, 0)), Texture0.Load(int3(x, i-m-1, 0)), alpha);
    }
}

StructuredBuffer<float4> SBuffer0 : register(t0);

// copy from structured buffer to texture
float4 ps_copy_uav(float4 pos : SV_POSITION, float2 tex : TEXCOORD0) : SV_TARGET
{
    uint2 pixelPos = floor(tex.xy * uint2(dstWidth, dstHeight));
    uint buffIndex = pixelPos.y*dstWidth + pixelPos.x;
    return SBuffer0[buffIndex];
}
