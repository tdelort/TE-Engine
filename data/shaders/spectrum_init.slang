import frame_constant_buffer;
import constants;

#include "constant_buffer.h"

struct material_data
{
    DescriptorHandle<Texture2D> gaussian_noise_tex;
    DescriptorHandle<RWTexture2D<float2>> output_tex;
    float patch_length;
};

DECLARE_MATERIAL_CONSTANT_BUFFER(material_data, g_material_cb);

/* 
 *	omega = 2 * M_PI / T = 2 * M_PI * f 
 *	k = length( K ) = 2 * M_PI / lambda
 *
 *	omega = g * k 
 *  (or a more accurate version taking the water depth h into account, but uneeded in our case)
 *	omega = g * k * tanh( k * h ) 
 */

//-----------------------------------------------------------------------------
float tma_correction(float _omega)
{
    float omega_h = _omega * sqrt(500.f / constants::g);
    if ( omega_h <= 1.f )
        return 0.5f * omega_h * omega_h;
    if ( omega_h < 2.f )
        return 1.f - 0.5f * (2.f - omega_h) * (2.f - omega_h);

    return 1.f;
}


float jonswap_spectrum(float _omega, float _wind_speed, float _fetch)
{
    const float alpha = 0.076f * pow(_wind_speed * _wind_speed / (_fetch * constants::g), 0.22f);  
    const float omega_p = 22.f * pow(constants::g * constants::g / (_fetch * _wind_speed), 1.f / 3.f);
    const float phi = 3.3f;

    const float sigma = _omega <= omega_p ? 0.07f : 0.09f;

    const float omega_diff = _omega - omega_p;
    const float sigma_omega_p = sigma * omega_p;
    const float r = exp(-(omega_diff * omega_diff) / (2.f * sigma_omega_p * sigma_omega_p) );

    const float omega_2 = _omega * _omega;
    const float omega_4 = omega_2 * omega_2;
    const float omega_5 = omega_4 * _omega;

    const float omega_ratio = (omega_p / _omega);
    const float omega_ratio_2 = omega_ratio * omega_ratio;
    const float omega_ratio_4 = omega_ratio_2 * omega_ratio_2;

    return /*tma_correction(_omega) * */ ( (alpha * constants::g * constants::g) / omega_5 ) * exp(-1.25f * omega_ratio_4) * pow(phi, r);
}

float ab_spectrum(float _omega, float _A, float _B)
{
    const float omega_rcp = 1.f / _omega;
    const float omega_rcp_2 = omega_rcp * omega_rcp;
    const float omega_rcp_4 = omega_rcp_2 * omega_rcp_2;

    // A / (omega ^ 5) * exp ( - B / (omega ^ 4 ))
    return (_A * omega_rcp_4 * omega_rcp) * exp(-_B * omega_rcp_4);
}

//-----------------------------------------------------------------------------
float pierson_moskovitz_spectrum(float _omega, float _wind_speed)
{
    const float alpha = 0.0081f;
    const float beta = 0.74f;
    const float omega_0 = constants::g / (1.026f * _wind_speed);

    return ab_spectrum(_omega, alpha * constants::g * constants::g, beta * omega_0 * omega_0 * omega_0 * omega_0);
}

//-----------------------------------------------------------------------------
float non_directional_spectrum(float _omega)
{
    float lambda = 2.f * constants::pi * constants::g / (_omega * _omega);

    float maxWaveLength = g_material_cb.patch_length * 0.66f;
    float minWaveLength = g_material_cb.patch_length * 0.025f;

    const float wind_speed = 7.5f; // in m/s
    const float fetch = 400.f * 1000.f; // in m

    return jonswap_spectrum(_omega, wind_speed, fetch);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void cs_spectrum_init(uint3 _thread_id : SV_DispatchThreadID)
{
    RWTexture2D<float2> output_tex = g_material_cb.output_tex;
    uint2 output_dims;
    output_tex.GetDimensions(output_dims.x, output_dims.y);

    if(any(_thread_id.xy > output_dims))
        return;

    // Common useful data
    const int2 c_tid = _thread_id.xy - output_dims / 2;
    const float delta_k = 2.f * constants::pi / (g_material_cb.patch_length);
    const float2 k = c_tid * delta_k;
    const float k_length = length(k);
    const float lambda = 2.f * constants::pi / k_length;
    const float omega = sqrt( constants::g * k_length );
    //const float domegadk = WAVE_SPECTRUM_G * 0.5f / omega
    const float domegadk = constants::g / ( 2.f * omega );

    // Random value
    Texture2D gaussian_noise_tex = g_material_cb.gaussian_noise_tex;
    uint2 gaussian_noise_tex_dim;
    gaussian_noise_tex.GetDimensions(gaussian_noise_tex_dim.x, gaussian_noise_tex_dim.y);

    const float2 R = gaussian_noise_tex[_thread_id.xy % gaussian_noise_tex_dim].rg;

    // Directionnal Spreading Function D(omega, theta)
    static const float D = 1.f / (2.f * constants::pi); 

    const float S = non_directional_spectrum(omega);

    const bool zero = (k_length == 0); // || (length(cTid) < 4));
    const float2 spectrum = 2.f * D * S * ( abs(domegadk) / k_length * delta_k * delta_k);

    output_tex[_thread_id.xy] = zero ? 0.f : R * sign(spectrum) * sqrt(abs(spectrum));
}