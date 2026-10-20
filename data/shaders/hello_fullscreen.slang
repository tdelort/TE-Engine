#include "constants.h"

import frame_constant_buffer;

// hello_fullscreen.slang
struct material_data
{
    // float4x4 model;
    uint2 viewportSize;                 // current draw call viewport size
    float brushSize;                    // brush size in pixels
    float fadeSpeed;                    // color fade speed in 1/seconds
    DescriptorHandle<Texture2D> input;  // input texture
};
ConstantBuffer<material_data> g_material_cb : register(MATERIAL_CONSTANT_BUFFER_REGISTER);


struct VsInput
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};
struct VsOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
VsOutput mainVS(VsInput input)
{
    VsOutput output;
    output.pos = float4(input.pos.xyz, 1.f);
    output.uv = input.uv;
    return output;
}

[shader("pixel")]
float4 mainPSWrite(VsOutput input) : SV_TARGET
{
    Texture2D inputTexture = g_material_cb.input; 
    float4 inputColor = inputTexture.Load(uint3(uint2(input.pos.xy), 0));
    const float4 color = float4(input.uv, 0.f, 1.f);
    return lerp(inputColor, color, saturate(1.f - length(input.pos.xy - g_frame_cb.mousePosition.xy) / g_material_cb.brushSize));
}

[shader("pixel")]
float4 mainPSFade(VsOutput input) : SV_TARGET
{
    Texture2D inputTexture = g_material_cb.input; 
    float4 inputColor = inputTexture.Load(uint3(uint2(input.pos.xy), 0));
    return saturate(inputColor - max(g_frame_cb.dt, 1.f / 1000.f) * g_material_cb.fadeSpeed);
}