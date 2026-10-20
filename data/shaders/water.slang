import frame_constant_buffer;
import samplers;

#include "constant_buffer.h"

struct material_data
{
    float4x4 model;
    float3 lambda;
    float patch_length;
    DescriptorHandle<Texture2D> waves_displacement_tex;
    DescriptorHandle<Texture2D> waves_derivatives_tex;
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
    float3 non_displaced_world_position : TEXCOORD1;
};

[shader("vertex")]
vs_output vs_water(vs_input input)
{
    Texture2D waves_displacement_tex = g_material_cb.waves_displacement_tex;

    // .-----.-----.-     -.-----.-----.
    // |     |     |  ...  |     |     |
    // .-----.-----.-     2X+4--2X+2--2X
    // |     |     |  ...  |  /  |  /  |
    //                    2X+3  2X+1  
    // 1-----3-----5-     2X-5--2X-3--2X-1
    // |  \  |  \  |  ...  |  \  |  \  |
    // 0-----2-----4-     2X-6--2X-4--2X-2
    //
    // X-1 * Y-1 squares 
    // 1 square == 6 vertex

    vs_output output;

    float4 world_position = mul(float4(input.position, 1.f), g_material_cb.model);
    output.non_displaced_world_position = world_position.xyz;

    float2 uv = world_position.xy / g_material_cb.patch_length;
    world_position.xyz += g_material_cb.lambda * waves_displacement_tex.SampleLevel(g_linear_wrap, uv, 0.f).xyz;
    output.world_position = world_position.xyz;

    float4 view_position = mul(world_position, g_frame_cb.camera_view);
    float4 proj_position = mul(view_position, g_frame_cb.camera_proj);

    output.clip_position = proj_position;
    return output;
}

[shader("pixel")]
float4 ps_water(vs_output input) : SV_TARGET
{
    Texture2D waves_derivatives_tex = g_material_cb.waves_derivatives_tex;

    const float2 uv = input.non_displaced_world_position.xy / g_material_cb.patch_length;
    // (d_x_dx, d_z_dx, d_y_dy, d_z_dy)
    const float4 derivatives = g_material_cb.lambda.xzyz * waves_derivatives_tex.SampleLevel(g_linear_wrap, uv, 0.f);
	const float3 slope = float3(derivatives.y / (1.f + derivatives.x), derivatives.w / (1.f + derivatives.z), 0.f);
    const float3 normal = normalize(float3(0.f, 0.f, 1.f) - slope);

    const float3 albedo = float3(0.1, 0.1, 0.9f);

    const float3 fake_sun_dir = normalize(float3(0.4f, 0.1f, 0.8f));

    const float3 color = saturate(dot(normal, fake_sun_dir) * albedo);

    // slope and normals from derivatives
    return float4(color, 1.f);
}
