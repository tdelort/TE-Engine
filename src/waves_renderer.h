#pragma once

#include <veer/display/render/command_buffer.h>
#include <veer/display/render/command_queue.h>

#include "frame_context.h"
#include "shader_utils.h"

namespace vdr = veer::display::render;
namespace vdw = veer::display::window;

namespace te
{
    class waves_renderer
    {
    public:
        waves_renderer(vdr::render_device& _device, vdr::constant_buffer_definition& _frame_constant_buffer_definition)
        {
            vdr::constant_buffer_definition spectrum_init_constant_buffer_definition({
                {"gaussian_noise_tex", {0u, 8u} },
                {"output_tex",         {8u, 8u} },
                {"patch_length",       {16u, 4u}},
                // + 4 padding
            });

            {
                m_spectrum_init_pass.m_technique = shader_utils::build_compute_shader_from_disk(
                    s_precompiled_cs_spectrum_init_path,
                    _device,
                    _frame_constant_buffer_definition,
                    spectrum_init_constant_buffer_definition
                );

                m_spectrum_init_pass.m_material_constant_buffer = veer::unique_ptr<vdr::constant_buffer>::make(_device, spectrum_init_constant_buffer_definition);

                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_init_pass, gaussian_noise_tex);
                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_init_pass, output_tex);
                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_init_pass, patch_length);
            }

            vdr::constant_buffer_definition spectrum_animate_constant_buffer_definition({
                {"input_tex",        {0u, 8u} },
                {"displacement_tex", {8u, 8u} },
                {"derivatives_tex",  {16u, 8u}},
                {"patch_length",     {24u, 4u}},
                // + 4 padding
            });

            {
                m_spectrum_animate_pass.m_technique = shader_utils::build_compute_shader_from_disk(
                    s_precompiled_cs_spectrum_animate_path,
                    _device,
                    _frame_constant_buffer_definition,
                    spectrum_animate_constant_buffer_definition
                );

                m_spectrum_animate_pass.m_material_constant_buffer = veer::unique_ptr<vdr::constant_buffer>::make(_device, spectrum_animate_constant_buffer_definition);

                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_animate_pass, input_tex);
                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_animate_pass, displacement_tex);
                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_animate_pass, derivatives_tex);
                VEER_FILL_PASS_PARAM_ID_HELPER(m_spectrum_animate_pass, patch_length);
            }
        }

        void update_render_targets(vdr::texture_2d_desc _backbuffer_desc, vdr::graphics_command_buffer& _command_buffer)
        {
        }

        void init_spectrum(frame_context& _ctx, vdr::render_thread& _render_thread)
        {
            vdr::command_queue& graphics_queue = _ctx.m_device.get_graphics_command_queue();
            vdr::graphics_command_buffer command_buffer(_render_thread);

            const frame_render_resource_context& resource_context = _ctx.m_resource_context;
            const auto& pass = m_spectrum_init_pass;
            {
                vdr::constant_buffer::update_context update_ctx(*pass.m_material_constant_buffer, _render_thread);

                update_ctx.set_constant(pass.m_params_id.m_patch_length, get_patch_length());

                vdr::render_device_texture_2d& gaussian_noise_tex = *resource_context.m_gaussian_noise_tex;
                update_ctx.set_texture_read_only(pass.m_params_id.m_gaussian_noise_tex, gaussian_noise_tex);

                vdr::render_device_texture_2d& spectrum_render_target = *resource_context.m_spectrum_t0_render_target;
                update_ctx.set_texture_read_write(pass.m_params_id.m_output_tex, spectrum_render_target);
            }

            {
                vdr::compute_submit_context ctx(command_buffer, *pass.m_technique);

                ctx.set_constant_buffer(*resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
                ctx.set_constant_buffer(*pass.m_material_constant_buffer, vdr::constant_buffer_type::material);

                // TODO
                // const veer::math::vec3u group_size = _compute.get_group_size();
                static constexpr veer::math::vec3u s_group_size(8u, 8u, 1u);

                const veer::math::vec2u output_size = resource_context.m_spectrum_t0_render_target->get_desc().m_size;
                const veer::math::vec3u output_size_3d(output_size.x(), output_size.y(), 1u);
                const veer::math::vec3u group_count = veer::math::ceiled_remainder(output_size_3d, s_group_size);

                ctx.dispatch(group_count);
            }

            graphics_queue.enqueue(std::move(command_buffer));
        }

        void animate_spectrum(frame_context& _ctx, vdr::render_thread& _render_thread)
        {
            vdr::command_queue& graphics_queue = _ctx.m_device.get_graphics_command_queue();
            vdr::graphics_command_buffer command_buffer(_render_thread);

            const frame_render_resource_context& resource_context = _ctx.m_resource_context;
            const auto& pass = m_spectrum_animate_pass;
            {
                vdr::constant_buffer::update_context update_ctx(*pass.m_material_constant_buffer, _render_thread);

                update_ctx.set_constant(pass.m_params_id.m_patch_length, get_patch_length());

                vdr::render_device_texture_2d& spectrum_t0_render_target = *resource_context.m_spectrum_t0_render_target;
                update_ctx.set_texture_read_only(pass.m_params_id.m_input_tex, spectrum_t0_render_target);

                vdr::render_device_texture_2d& displacement_spectrum_render_target = *resource_context.m_displacement_spectrum_render_target;
                update_ctx.set_texture_read_write(pass.m_params_id.m_displacement_tex, displacement_spectrum_render_target);

                vdr::render_device_texture_2d& derivatives_spectrum_render_target = *resource_context.m_derivatives_spectrum_render_target;
                update_ctx.set_texture_read_write(pass.m_params_id.m_derivatives_tex, derivatives_spectrum_render_target);
            }

            {
                vdr::compute_submit_context ctx(command_buffer, *pass.m_technique);

                ctx.set_constant_buffer(*resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
                ctx.set_constant_buffer(*pass.m_material_constant_buffer, vdr::constant_buffer_type::material);

                // TODO
                // const veer::math::vec3u group_size = _compute.get_group_size();
                static constexpr veer::math::vec3u s_group_size(8u, 8u, 1u);

                veer::math::vec2u output_size = resource_context.m_displacement_spectrum_render_target->get_desc().m_size;
                const veer::math::vec3u output_size_3d(output_size.x(), output_size.y(), 1u);
                const veer::math::vec3u group_count = veer::math::ceiled_remainder(output_size_3d, s_group_size);

                ctx.dispatch(group_count);
            }

            graphics_queue.enqueue(std::move(command_buffer));
        }

        float get_patch_length() const
        {
            return 128.f;
        }

    private:
        static constexpr const char* s_precompiled_cs_spectrum_init_path = "cs_spectrum_init.bin";
        static constexpr const char* s_precompiled_cs_spectrum_animate_path = "cs_spectrum_animate.bin";

        struct spectrum_init_params
        {
            vdr::buffer_elem_info m_gaussian_noise_tex;
            vdr::buffer_elem_info m_output_tex;
            vdr::buffer_elem_info m_patch_length;
        };
        shader_utils::compute_pass<spectrum_init_params> m_spectrum_init_pass;

        struct spectrum_animate_params
        {
            vdr::buffer_elem_info m_input_tex;
            vdr::buffer_elem_info m_displacement_tex;
            vdr::buffer_elem_info m_derivatives_tex;
            vdr::buffer_elem_info m_patch_length;
        };
        shader_utils::compute_pass<spectrum_animate_params> m_spectrum_animate_pass;
    };
} // namespace te