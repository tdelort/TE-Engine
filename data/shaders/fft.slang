// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

/*********************************************************************
This file is from the FFT Implementation for NVIDIA Direct3D SDK Sample : OceanCS
It is largely modified to fit my needs (added Radix 2 / 4 versions. Changed to work on 2D textures as input / output)
**********************************************************************/

import frame_constant_buffer;

#include "constant_buffer.h"

struct material_data
{
    uint thread_count;
    uint o_stride;
    uint i_stride;
    uint p_stride;
    float phase_base;
    uint dimension;
    DescriptorHandle<Texture2D> input_tex;
    DescriptorHandle<RWTexture2D> output_tex;
};

DECLARE_MATERIAL_CONSTANT_BUFFER(material_data, g_material_cb);

#define COS_PI_4_16 0.70710678118654752440084436210485f
#define TWIDDLE_1_8 COS_PI_4_16, -COS_PI_4_16
#define TWIDDLE_3_8 -COS_PI_4_16, -COS_PI_4_16

#define THREAD_GROUP_SIZE 32

#define RADIX 8 

interface fft
{
    static const uint s_radix;
    static const uint s_log_radix;
    static const uint s_rev_bit_order_array[s_radix];

    static void forward(inout float4 _data[s_radix]);
    static void twiddle(inout float4 _data[s_radix], float _phase);
}

namespace fft_helpers
{
    void ft2(inout float4 a, inout float4 b)
    {
        float2 t;

        t = a.xz;
        a.xz += b.xz;
        b.xz = t - b.xz;

        t = a.yw;
        a.yw += b.yw;
        b.yw = t - b.yw;
    }

    void cmul_forward(inout float4 a, float2 bx, float2 by)
    {
        float2 t = a.xz;
        a.xz = t * bx - a.yw * by;
        a.yw = t * by + a.yw * bx;
    }

    void upd_forward(inout float4 a, inout float4 b)
    {
        float2 A = a.xz;
        float2 B = b.yw;

        a.xz += b.yw;
        b.yw = a.yw + b.xz;
        a.yw -= b.xz;
        b.xz = A - B;
    }

    void twiddle_impl(inout float4 d, float phase)
    {
        float tx, ty;

        sincos(phase, ty, tx);
        float2 t = d.xz;
        d.xz = t * tx - d.yw * ty;
        d.yw = t * ty + d.yw * tx;
    }
};

struct fft_radix_2 : fft
{
    static const uint s_radix = 2u;
    static const uint s_log_radix = 1u;
    static const uint s_rev_bit_order_array[s_radix] = {0u, 1u};

    static void forward(inout float4 _data[s_radix])
    {
        fft_helpers::ft2(_data[0], _data[1]);
    }

    static void twiddle(inout float4 _data[s_radix], float _phase)
    {
        for(size_t i = 1; i < s_radix; ++i)
        {
            fft_helpers::twiddle_impl(_data[s_rev_bit_order_array[i]], i * _phase);
        }
    }
};



struct fft_radix_4 : fft
{
    static const uint s_radix = 4u;
    static const uint s_log_radix = 2u;
    static const uint s_rev_bit_order_array[s_radix] = {0u, 2u, 1u, 3u};

    static void forward(inout float4 _data[s_radix])
    {
        fft_helpers::ft2(_data[0], _data[2]);
        fft_helpers::ft2(_data[1], _data[3]);
        fft_helpers::ft2(_data[0], _data[1]);

        fft_helpers::upd_forward(_data[2], _data[3]);
    }

    static void twiddle(inout float4 _data[s_radix], float _phase)
    {
        for(size_t i = 1; i < s_radix; ++i)
        {
            fft_helpers::twiddle_impl(_data[s_rev_bit_order_array[i]], i * _phase);
        }
    }
};



struct fft_radix_8 : fft
{
    static const uint s_radix = 8u;
    static const uint s_log_radix = 3u;
    static const uint s_rev_bit_order_array[s_radix] = {0u, 4u, 2u, 6u, 1u, 5u, 3u, 7u};

    static void forward(inout float4 _data[s_radix])
    {
        fft_helpers::ft2(_data[0], _data[4]);
        fft_helpers::ft2(_data[1], _data[5]);
        fft_helpers::ft2(_data[2], _data[6]);
        fft_helpers::ft2(_data[3], _data[7]);

        fft_helpers::upd_forward(_data[4], _data[6]);
        fft_helpers::upd_forward(_data[5], _data[7]);

        fft_helpers::cmul_forward(_data[5], TWIDDLE_1_8);
        fft_helpers::cmul_forward(_data[7], TWIDDLE_3_8);

        forward_4(_data);
        fft_helpers::ft2(_data[4], _data[5]);
        fft_helpers::ft2(_data[6], _data[7]);
    }

    static void twiddle(inout float4 _data[s_radix], float _phase)
    {
        for(size_t i = 1; i < s_radix; ++i)
        {
            fft_helpers::twiddle_impl(_data[s_rev_bit_order_array[i]], i * _phase);
        }
    }

    private static void forward_4(inout float4 _data[s_radix])
    {
        fft_helpers::ft2(_data[0], _data[2]);
        fft_helpers::ft2(_data[1], _data[3]);
        fft_helpers::ft2(_data[0], _data[1]);

        fft_helpers::upd_forward(_data[2], _data[3]);
    }
};

#if RADIX == 2
#define FFT_OPERATOR fft_radix_2
#endif // RADIX == 2
#if RADIX == 4
#define FFT_OPERATOR fft_radix_4
#endif // RADIX == 4
#if RADIX == 8
#define FFT_OPERATOR fft_radix_8
#endif // RADIX == 8

void compute<T : fft>(uint3 _thread_id, bool _last_pass, uint _o_stride, uint _i_stride, uint _p_stride, float _phase_base, uint _dimension, Texture2D _in, RWTexture2D _out)
{
    float4 data[T::s_radix];

    const uint imod = _thread_id.x & (_i_stride - 1); // _thread_id.x % i_stride
    const uint iaddr = ((_thread_id.x - imod) << T::s_log_radix) + imod;

    for (uint i = 0; i < T::s_radix; ++i)
    {
        //data[i] = _in[iaddr + i * i_stride];

        const uint tmp = iaddr + i * _i_stride;
        const uint y = tmp / _dimension;
        const uint x = tmp % _dimension;
        data[i] = _in[uint2(x, y)];
    }

    T::forward(data);

    if (!_last_pass)
    {
        const uint p = _thread_id.x & (_i_stride - _p_stride);
        const float phase = _phase_base * (float)p;
        T::twiddle(data, phase);
    }

    // Store the result
    const uint omod = _thread_id.x & (_o_stride - 1);
    const uint oaddr = ((_thread_id.x - omod) << T::s_log_radix) + omod;

    for (uint i = 0; i < T::s_radix; i++)
    {
        const uint tmp = oaddr + i * _o_stride;
        const uint y = tmp / _dimension;
        const uint x = tmp % _dimension;

        // We need a sign correction since we use a FFT centered on the center pixel and not the top left corner
        // This induces a shift in phase that needs to be fixed before returning the last pass
        const int sign_correction = (_last_pass && bool((x + y) & 1)) ? -1 : 1;
        _out[uint2(x, y)] = data[T::s_rev_bit_order_array[i]] * sign_correction;
    }
}

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void cs_fft_radix_8_main(uint3 _thread_id : SV_DispatchThreadID)
{
    if (_thread_id.x >= g_material_cb.thread_count)
        return;

    compute<FFT_OPERATOR>(_thread_id, false, g_material_cb.o_stride, g_material_cb.i_stride, g_material_cb.p_stride, g_material_cb.phase_base, g_material_cb.dimension, g_material_cb.input_tex, g_material_cb.output_tex);
}

[shader("compute")]
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void cs_fft_radix_8_last(uint3 _thread_id : SV_DispatchThreadID)
{
    if (_thread_id.x >= g_material_cb.thread_count)
        return;

    compute<FFT_OPERATOR>(_thread_id, true, g_material_cb.o_stride, g_material_cb.i_stride, g_material_cb.p_stride, g_material_cb.phase_base, g_material_cb.dimension, g_material_cb.input_tex, g_material_cb.output_tex);
}