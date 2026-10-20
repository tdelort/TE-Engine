import frame_constant_buffer;
import samplers;

#include "constant_buffer.h"

struct material_data
{
    uint2 viewport_size;
    DescriptorHandle<Texture2D> input_tex;
};

DECLARE_MATERIAL_CONSTANT_BUFFER(material_data, g_material_cb);

struct vs_input
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};
struct vs_output
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
vs_output vs_blit(vs_input input)
{
    vs_output output;
    output.pos = float4(input.pos.xyz, 1.f);
    output.uv = input.uv;
    return output;
}

[shader("pixel")]
float4 ps_blit(vs_output input) : SV_TARGET
{
    const int2 texel = /*g_material_cb.viewport_size.xy **/ int2(input.pos.xy);
    
    // const float2 ratios = g_material_cb.viewport_size.xy / g_material_cb.viewport_size.yx;
    // const float2 input_texture_uv = input.uv * max(ratios, 1.f);
    // if (any(input_texture_uv > 1.f))
    // {
    //     return float4(0.f);
    // }

    Texture2D input_tex = g_material_cb.input_tex;
    uint2 input_tex_dims;
    input_tex.GetDimensions(input_tex_dims.x, input_tex_dims.y);

    //const float4 color = g_material_cb.input_tex.Sample(g_linear_clamp, input_texture_uv);
    const float4 color = all(texel < input_tex_dims) ? input_tex.Load(int3(texel.xy, 0)) : float4(0);
    return color;
}