import frame_constant_buffer;
import samplers;

#include "constant_buffer.h"

struct material_data
{
    float4x4 model;
    float3 albedo;
};

DECLARE_MATERIAL_CONSTANT_BUFFER(material_data, g_material_cb);

struct vs_input
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct vs_output
{
    float4 clip_position : SV_POSITION;
    float3 world_position : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

[shader("vertex")]
vs_output vs_placeholder(vs_input input)
{
    float4 world_position = mul(float4(input.position, 1.f), g_material_cb.model);
    float4 view_position = mul(world_position, g_frame_cb.camera_view);
    float4 proj_position = mul(view_position, g_frame_cb.camera_proj);

    vs_output output;
    output.clip_position = proj_position;
    output.world_position = world_position.xyz;
    output.uv = input.uv;
    return output;
}

[shader("pixel")]
float4 ps_placeholder(vs_output input) : SV_TARGET
{
    float3 color = lerp(g_material_cb.albedo, input.world_position * 0.01f, 0.5f);
    return float4(color, 1.f);
}
