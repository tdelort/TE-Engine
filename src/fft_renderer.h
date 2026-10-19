#pragma once

#include <veer/core/math/math.h>
#include <veer/core/math/vec.h>
#include <veer/core/unique_ptr.h>

#include "frame_context.h"

namespace veer::display::render
{
    class render_thread;
    class render_device;
    class render_device_texture_2d;
    class compute_technique;
    class compute_command_buffer;
    class command_queue;
} // namespace veer::display::render
namespace vdr = veer::display::render;

namespace te
{
    class fft_renderer
    {
    public:
        fft_renderer(vdr::render_device& _device, vdr::constant_buffer_definition& _frame_constant_buffer_definition);

        void compute_fft_512_512(frame_context& _ctx, vdr::render_thread& _render_thread, vdr::render_device_texture_2d& _in, vdr::render_device_texture_2d& _out);

    private:
        void fft_pass(
            frame_context& _frame_ctx,
            vdr::render_thread& _render_thread,
            vdr::compute_technique& _compute,
            size_t _thread_count,
            size_t _o_stride,
            size_t _i_stride,
            size_t _p_stride,
            double _phase_base,
            unsigned int _dimension,
            vdr::render_device_texture_2d& _in,
            vdr::render_device_texture_2d& _out,
            vdr::compute_command_buffer& _command_buffer
        );

    private:
        veer::unique_ptr<vdr::constant_buffer> m_fft_constant_buffer;

        // cached params id
        struct fft_constant_buffer_elems
        {
            vdr::buffer_elem_info m_thread_count;
            vdr::buffer_elem_info m_o_stride;
            vdr::buffer_elem_info m_i_stride;
            vdr::buffer_elem_info m_p_stride;
            vdr::buffer_elem_info m_phase_base;
            vdr::buffer_elem_info m_dimension;
            vdr::buffer_elem_info m_input_tex;
            vdr::buffer_elem_info m_output_tex;
        };
        fft_constant_buffer_elems m_constant_buffer_elems;

        static constexpr const char* s_precompiled_cs_fft_radix_8_main_path = "cs_fft_radix_8_main.bin";
        static constexpr const char* s_precompiled_cs_fft_radix_8_last_path = "cs_fft_radix_8_last.bin";

        veer::unique_ptr<vdr::compute_technique> m_fft_radix_8_technique;
        veer::unique_ptr<vdr::compute_technique> m_fft_radix_8_last_technique;
    };
} // namespace te