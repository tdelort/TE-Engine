#include "fft_renderer.h"

#include "shader_utils.h"

#include <veer/core/unique_ptr.h>
#include <veer/display/render/command_buffer.h>
#include <veer/display/render/command_queue.h>
#include <veer/display/render/compute_technique.h>
#include <veer/display/render/render_device.h>
#include <veer/display/render/render_thread.h>

namespace vdr = veer::display::render;

namespace te
{
    fft_renderer::fft_renderer(vdr::render_device& _device, vdr::constant_buffer_definition& _frame_constant_buffer_definition)
    {
        vdr::constant_buffer_definition fft_constant_buffer_definition({
            {"thread_count", {0u, 4u} },
            {"o_stride",     {4u, 4u} },
            {"i_stride",     {8u, 4u} },
            {"p_stride",     {12u, 4u}},
            {"phase_base",   {16u, 4u}},
            {"dimension",    {20u, 4u}},
            {"input_tex",    {24u, 8u}},
            {"output_tex",   {32u, 8u}},
        });

        m_fft_radix_8_technique =
            shader_utils::build_compute_shader_from_disk(s_precompiled_cs_fft_radix_8_main_path, _device, _frame_constant_buffer_definition, fft_constant_buffer_definition);

        m_fft_radix_8_last_technique =
            shader_utils::build_compute_shader_from_disk(s_precompiled_cs_fft_radix_8_last_path, _device, _frame_constant_buffer_definition, fft_constant_buffer_definition);

        m_fft_constant_buffer = veer::unique_ptr<vdr::constant_buffer>::make(_device, fft_constant_buffer_definition, "fft_constant_buffer");

        // For now, this looks really stupid, but when the cb definition will come from shader reflection, it will make
        // more sense

        m_constant_buffer_elems.m_thread_count = fft_constant_buffer_definition.get_elem_info("thread_count");
        m_constant_buffer_elems.m_o_stride = fft_constant_buffer_definition.get_elem_info("o_stride");
        m_constant_buffer_elems.m_i_stride = fft_constant_buffer_definition.get_elem_info("i_stride");
        m_constant_buffer_elems.m_p_stride = fft_constant_buffer_definition.get_elem_info("p_stride");
        m_constant_buffer_elems.m_phase_base = fft_constant_buffer_definition.get_elem_info("phase_base");
        m_constant_buffer_elems.m_dimension = fft_constant_buffer_definition.get_elem_info("dimension");
        m_constant_buffer_elems.m_input_tex = fft_constant_buffer_definition.get_elem_info("input_tex");
        m_constant_buffer_elems.m_output_tex = fft_constant_buffer_definition.get_elem_info("output_tex");
    }

    void fft_renderer::compute_fft_512_512(frame_context& _ctx, vdr::render_thread& _render_thread, vdr::render_device_texture_2d& _in, vdr::render_device_texture_2d& _out)
    {
        // TODO : lets do allat on the main graphics queue (it's a bit early in to use the compute queue for now. I
        // still have some more things to iron out before)
        vdr::command_queue& graphics_queue = _ctx.m_device.get_graphics_command_queue();
        vdr::graphics_command_buffer command_buffer(_render_thread);

        // TODO : you know how to generalize it to more dimensions. But it takes time and I just want to see pretty
        // pixels (but pinky promise I'll do it one day ;3)
        static constexpr size_t s_dimension = 512u;
        static constexpr size_t s_dimension_sqr = s_dimension * s_dimension;

        // TODO : same here, I could generalize a bit, but for 512x512 texture this is all I need
        static constexpr size_t s_radix = 8u;

        size_t o_stride_base = s_dimension_sqr;
        size_t i_stride = s_dimension_sqr;
        size_t p_stride = s_dimension;
        double phase_base = (-2.0 * veer::math::s_pi) / static_cast<double>(s_dimension_sqr);

        static constexpr size_t s_thread_count = s_dimension_sqr / s_radix;

        bool data_in_input_texture = true;
        for (size_t dim = 0; dim < 2; ++dim)
        {
            for (size_t pass = 0; pass < 3; ++pass)
            {
                const size_t o_stride = o_stride_base / s_radix;
                i_stride /= s_radix;

                vdr::render_device_texture_2d& in_texture = data_in_input_texture ? _in : _out;
                vdr::render_device_texture_2d& out_texture = data_in_input_texture ? _out : _in;
                vdr::compute_technique& technique = (i_stride == 1 ? *m_fft_radix_8_last_technique : *m_fft_radix_8_technique);

                fft_pass(_ctx, _render_thread, technique, s_thread_count, o_stride, i_stride, p_stride, phase_base, s_dimension, in_texture, out_texture, command_buffer);

                data_in_input_texture = !data_in_input_texture;

                phase_base *= static_cast<double>(s_radix);
            }

            o_stride_base = s_dimension;
            p_stride = 1u;
        }

        // if the data is in the _in texture, copy to out (happens if there is an even number of passes)
        if (data_in_input_texture)
        {
            vdr::copy_submit_context copy_ctx(command_buffer);
            copy_ctx.copy_texture(_out, _in);
        }

        graphics_queue.enqueue(std::move(command_buffer));
    }

    void fft_renderer::fft_pass(
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
    )
    {
        {
            vdr::constant_buffer::update_context update_ctx(*m_fft_constant_buffer, _render_thread);

            update_ctx.set_constant(m_constant_buffer_elems.m_thread_count, _thread_count);
            update_ctx.set_constant(m_constant_buffer_elems.m_o_stride, _o_stride);
            update_ctx.set_constant(m_constant_buffer_elems.m_i_stride, _i_stride);
            update_ctx.set_constant(m_constant_buffer_elems.m_p_stride, _p_stride);
            update_ctx.set_constant(m_constant_buffer_elems.m_phase_base, (float)_phase_base);
            update_ctx.set_constant(m_constant_buffer_elems.m_dimension, _dimension);

            update_ctx.set_texture_read_only(m_constant_buffer_elems.m_input_tex, _in);
            update_ctx.set_texture_read_write(m_constant_buffer_elems.m_output_tex, _out);
        }

        {
            vdr::compute_submit_context ctx(_command_buffer, _compute);

            ctx.set_constant_buffer(*_frame_ctx.m_resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
            ctx.set_constant_buffer(*m_fft_constant_buffer, vdr::constant_buffer_type::material);

            // TODO
            // const veer::math::vec3u group_size = _compute.get_group_size();
            static constexpr veer::math::vec3u s_group_size(32u, 1u, 1u);

            const veer::math::vec3u group_count = veer::math::ceiled_remainder(veer::math::vec3u(static_cast<unsigned int>(_thread_count), 1u, 1u), s_group_size);
            ctx.dispatch(group_count);
        }
    }
} // namespace te
