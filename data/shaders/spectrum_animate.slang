import frame_constant_buffer;
import constants;

#include "constant_buffer.h"

struct material_data
{
    DescriptorHandle<Texture2D> input_tex;
    DescriptorHandle<RWTexture2D<float4>> displacement_tex;
    DescriptorHandle<RWTexture2D<float4>> derivatives_tex;
    float patch_length;
};

DECLARE_MATERIAL_CONSTANT_BUFFER(material_data, g_material_cb);

//-----------------------------------------------------------------------------
float2 tangle(float2 _a, float2 _b)
{
    return float2(_a.x - _b.y, _a.y + _b.x);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_spectrum_animate(uint3 _thread_id : SV_DispatchThreadID)
{
    RWTexture2D<float4> displacement_tex = g_material_cb.displacement_tex;
    uint2 displacement_tex_dims;
    displacement_tex.GetDimensions(displacement_tex_dims.x, displacement_tex_dims.y);

    if(any(_thread_id.xy > displacement_tex_dims))
        return;

    const int2 half_output_dims = displacement_tex_dims / 2;

    // compute omega for current pixel
    const int2 c_tid = _thread_id.xy - half_output_dims;
    const float delta_k = 2.f * constants::pi / (g_material_cb.patch_length);
    const float2 k = c_tid * delta_k;
    const float k_length = length(k);
    const float omega = sqrt(constants::g * k_length);

    // Loading h0(k) and h0(-k) (they are complex numbers, x : real and y : img)
    Texture2D input_tex = g_material_cb.input_tex;
    const float2 h0_k = input_tex[_thread_id.xy].rg;
    const float2 h0_mk = input_tex[-c_tid + half_output_dims].rg;

    // Then, from Tessendorf's paper (section 4.4) : h(k,t) = h0(k) e^(i omega(k) t) + h0*(mk) e^(-i omega(k) t)

    float sin_omega_t_v, cos_omega_t_v;
    sincos(omega * g_frame_cb.time, sin_omega_t_v, cos_omega_t_v);

    // e^(ix) = cos(x) + i sin(x)
    // cos(-x) = cos(x),  sin(-x) = -sin(x)

    // above formula expanded using Eulers formula and sin/cos identities gives :
    // h(k,t) = (h0_k.x  + i * h0_k.y ) * (cos_omega_t_v - i * sin_omega_t_v)
    //		  + (h0_mk.x + i * h0_mk.y) * (cos_omega_t_v + i * sin_omega_t_v)

    // Then I hope I didn't mess up but it should end up looking like this :
    const float2 ht = float2(
        (h0_k.x + h0_mk.x) * cos_omega_t_v - (h0_k.y + h0_mk.y) * sin_omega_t_v,
        (h0_k.x - h0_mk.x) * sin_omega_t_v + (h0_k.y - h0_mk.y) * cos_omega_t_v
    );

    // From Tessendorf's paper (section 4.6 eq 44). Also very well explained in :
    // https://www.youtube.com/watch?v=kGEqaX4Y4bQ
    const float2 normalized_k = k_length == 0 ? 0.f : (k / k_length);
    const float2 iht = float2(ht.y, -ht.x); // i * h(k,t)
    const float2 d_x = normalized_k.x * iht;
    const float2 d_y = normalized_k.y * iht;
    
    const float2 d_x_dx = -ht * k.x * normalized_k.x;
    const float2 d_y_dy = -ht * k.y * normalized_k.y;
    // const float2 d_x_dy = -ht * k.y * normalized_k.x;

    const float2 d_ht_dx = iht * k.x;
    const float2 d_ht_dy = iht * k.x;

    displacement_tex[_thread_id.xy] = float4(tangle(d_x, d_y), ht);
    g_material_cb.derivatives_tex[_thread_id.xy] = float4(tangle(d_x_dx, d_ht_dx), tangle(d_y_dy, d_ht_dy));
}