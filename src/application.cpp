#include "application.h"

#include <veer/core/debug.h>

#if defined(_DEBUG)
#include <veer/core/math/tests/mat_tests.h>
#include <veer/core/math/tests/transform_tests.h>
#include <veer/core/math/tests/vec_tests.h>

#endif // defined(_DEBUG)

#include <veer/core/containers/span.h>
#include <veer/core/containers/static_array.h>
#include <veer/core/core.h>
#include <veer/core/debug.h>
#include <veer/core/math/mat.h>
#include <veer/core/math/projection.h>
#include <veer/core/math/transform.h>
#include <veer/core/math/vec.h>
#include <veer/core/unique_ptr.h>

#include <veer/display/render/base_types.h>
#include <veer/display/render/command_queue.h>
#include <veer/display/render/compute_technique.h>
#include <veer/display/render/graphics_technique.h>
#include <veer/display/render/render_device.h>
#include <veer/display/render/render_device_buffer.h>
#include <veer/display/render/render_device_texture_2d.h>
#include <veer/display/render/render_thread.h>
#include <veer/display/render/shader_source.h>
#include <veer/display/render/swap_chain.h>
#include <veer/display/render/technique.h>

#include <veer/display/window/windowing_service.h>

// #if defined(GLFW_WINDOW_BACKEND)
#include <veer/display/window/glfw/glfw_windowing_service.h>
// #endif // defined(GLFW_WINDOW_BACKEND)

// TODO don't leave here
#include <chrono>
#include <filesystem>
#include <fstream>

#include "fft_renderer.h"
#include "mesh_utils.h"
#include "render_resource_context.h"
#include "shader_utils.h"
#include "waves_renderer.h"

namespace te
{

    namespace vdr = veer::display::render;
    namespace vdw = veer::display::window;

    application::application()
    {
        m_window_service = veer::unique_ptr<vdw::glfw_windowing_service>::make();

        m_render_device = veer::unique_ptr<vdr::render_device>::make();
        m_render_thread = veer::unique_ptr<vdr::render_thread>::make(*m_render_device);

        VEER_LOG("Create window");
        create_window();

        VEER_LOG("Create swap chain");
        create_swap_chain();

        VEER_LOG("Create shader techniques");
        build_shader_techniques();

        {
            vdr::constant_buffer_definition frame_constant_buffer_definition;
            frame_constant_buffer_definition.m_elements.add(s_frame_constant_buffer_elems.cbegin(), s_frame_constant_buffer_elems.cend());

            m_fft_renderer = veer::unique_ptr<fft_renderer>::make(*m_render_device, frame_constant_buffer_definition);
            m_waves_renderer = veer::unique_ptr<waves_renderer>::make(*m_render_device, frame_constant_buffer_definition);
        }
    }

    void application::create_window()
    {
        const veer::math::vec2u initial_win_size(640u, 480u);
        m_window_service->create_window(initial_win_size);
    }

    void application::create_swap_chain()
    {
        vdw::window& window = m_window_service->get_window(0);
        m_swap_chain = m_render_device->alloc<vdr::swap_chain>(window);
    }

    enum class input_layout_type
    {
        pos_rgb32 = 0,
        pos_rgb32_uv_rg32,

        COUNT,
    };

    using input_layout_t = veer::containers::resizable_array<vdr::shader_signature::input_elem>;
    using output_layout_t = veer::containers::resizable_array<vdr::shader_signature::output_elem>;

    void application::build_shader_techniques()
    {
        // different shader input layouts
        input_layout_t input_layouts[] = {
            {{"POSITION", 0, vdr::render_device_data_format::r32g32b32_float, 0u}}, // pos_rgb32
            {{"POSITION", 0, vdr::render_device_data_format::r32g32b32_float, 0u}, {"TEXCOORD", 0, vdr::render_device_data_format::r32g32_float, 12u}}, // pos_rgb32_uv_rg32
        };

        vdr::constant_buffer_definition frame_constant_buffer_definition;
        frame_constant_buffer_definition.m_elements.add(s_frame_constant_buffer_elems.cbegin(), s_frame_constant_buffer_elems.cend());

        {
            vdr::constant_buffer_definition material_constant_buffer_definition({
                {"viewport_size", {0u, 8u}},
                {"input_tex",     {8u, 8u}},
            });

            output_layout_t color_output = {{vdr::render_device_data_format::r8g8b8a8_unorm}};

            vdr::shader_signature shader_signature(input_layouts[static_cast<size_t>(input_layout_type::pos_rgb32_uv_rg32)], nullptr, color_output);

            vdr::blend_state shader_blend_state = {};    // default
            vdr::rasterizer_state rasterizer_state = {}; // default
            vdr::shader_render_state shader_render_state({shader_blend_state}, rasterizer_state, {}, false);

            vdr::shader_stage_source_container_t blit_source_code = {};
            blit_source_code[static_cast<size_t>(vdr::shader_stage_type::vertex)] = shader_utils::read_shader_from_disk(s_precompiled_vs_blit_path);
            blit_source_code[static_cast<size_t>(vdr::shader_stage_type::pixel)] = shader_utils::read_shader_from_disk(s_precompiled_ps_blit_path);

            m_blit_pass.m_technique = veer::unique_ptr<vdr::graphics_technique>::make(
                frame_constant_buffer_definition,
                material_constant_buffer_definition,
                *m_render_device,
                blit_source_code,
                shader_signature,
                shader_render_state
            );

            m_blit_pass.m_material_constant_buffer =
                veer::unique_ptr<vdr::constant_buffer>::make(*m_render_device, material_constant_buffer_definition, "blit_pass_constant_buffer");

            VEER_FILL_PASS_PARAM_ID_HELPER(m_blit_pass, viewport_size);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_blit_pass, input_tex);
        }

        {
            vdr::constant_buffer_definition material_constant_buffer_definition({
                {"model",  {0u, 64u} },
                {"albedo", {64u, 12u}},
            });
            output_layout_t color_output = {{vdr::render_device_data_format::r8g8b8a8_unorm}};
            vdr::shader_signature::output_elem depth_output{vdr::render_device_data_format::d32_float};
            vdr::shader_signature shader_signature(input_layouts[static_cast<size_t>(input_layout_type::pos_rgb32_uv_rg32)], &depth_output, color_output);

            vdr::blend_state shader_blend_state = {};    // default
            vdr::rasterizer_state rasterizer_state = {}; // default
            vdr::depth_state depth_state = {.m_access = vdr::access_mode::read | vdr::access_mode::write, .m_func = vdr::depth_state::func::greater};
            vdr::shader_render_state shader_render_state({shader_blend_state}, rasterizer_state, depth_state, false);

            vdr::shader_stage_source_container_t placeholder_source_code = {};
            placeholder_source_code[static_cast<size_t>(vdr::shader_stage_type::vertex)] = shader_utils::read_shader_from_disk(s_precompiled_vs_placeholder_path);
            placeholder_source_code[static_cast<size_t>(vdr::shader_stage_type::pixel)] = shader_utils::read_shader_from_disk(s_precompiled_ps_placeholder_path);

            m_placeholder_pass.m_technique = veer::unique_ptr<vdr::graphics_technique>::make(
                frame_constant_buffer_definition,
                material_constant_buffer_definition,
                *m_render_device,
                placeholder_source_code,
                shader_signature,
                shader_render_state
            );

            m_placeholder_pass.m_material_constant_buffer =
                veer::unique_ptr<vdr::constant_buffer>::make(*m_render_device, material_constant_buffer_definition, "placeholder_pass_constant_buffer");

            VEER_FILL_PASS_PARAM_ID_HELPER(m_placeholder_pass, model);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_placeholder_pass, albedo);
        }

        {
            vdr::constant_buffer_definition material_constant_buffer_definition({
                {"model",                  {0u, 64u} },
                {"lambda",                 {64u, 12u}},
                {"patch_length",           {76u, 4u} },
                {"waves_displacement_tex", {80u, 8u} },
                {"waves_derivatives_tex",  {88u, 8u} },
            });
            output_layout_t color_output = {{vdr::render_device_data_format::r8g8b8a8_unorm}};
            vdr::shader_signature::output_elem depth_output{vdr::render_device_data_format::d32_float};
            vdr::shader_signature shader_signature(input_layouts[static_cast<size_t>(input_layout_type::pos_rgb32_uv_rg32)], &depth_output, color_output);

            vdr::blend_state shader_blend_state = {};    // default
            vdr::rasterizer_state rasterizer_state = {}; // default
            vdr::depth_state depth_state = {.m_access = vdr::access_mode::read | vdr::access_mode::write, .m_func = vdr::depth_state::func::greater};
            vdr::shader_render_state shader_render_state({shader_blend_state}, rasterizer_state, depth_state, false);

            vdr::shader_stage_source_container_t water_source_code = {};
            water_source_code[static_cast<size_t>(vdr::shader_stage_type::vertex)] = shader_utils::read_shader_from_disk(s_precompiled_vs_water_path);
            water_source_code[static_cast<size_t>(vdr::shader_stage_type::pixel)] = shader_utils::read_shader_from_disk(s_precompiled_ps_water_path);

            m_water_pass.m_technique = veer::unique_ptr<vdr::graphics_technique>::make(
                frame_constant_buffer_definition,
                material_constant_buffer_definition,
                *m_render_device,
                water_source_code,
                shader_signature,
                shader_render_state
            );

            m_water_pass.m_material_constant_buffer =
                veer::unique_ptr<vdr::constant_buffer>::make(*m_render_device, material_constant_buffer_definition, "water_pass_constant_buffer");

            VEER_FILL_PASS_PARAM_ID_HELPER(m_water_pass, model);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_water_pass, lambda);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_water_pass, patch_length);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_water_pass, waves_displacement_tex);
            VEER_FILL_PASS_PARAM_ID_HELPER(m_water_pass, waves_derivatives_tex);
        }
    }

    void application::build_geometry()
    {
        VEER_LOG("build quad");
        {
            //  1----2
            //  |  / |  y
            //  | /  |  |
            //  0----3  o--x
            static float fullscreen_quad_vb_data[] = {
                -1.f, -1.f, 0.5f, 0.f, 0.f, // 0
                -1.f, 1.f,  0.5f, 0.f, 1.f, // 1
                1.f,  1.f,  0.5f, 1.f, 1.f, // 2
                1.f,  -1.f, 0.5f, 1.f, 0.f  // 3
            };
            static uint32_t fullscreen_quad_ib_data[] = {0, 1, 2, 2, 3, 0};

            m_quad_mesh = mesh_utils::build_mesh(
                *m_render_device,
                4u,
                veer::containers::span<float>(fullscreen_quad_vb_data, VEER_STATIC_ARRAY_SIZE(fullscreen_quad_vb_data)),
                veer::containers::span<uint32_t>(fullscreen_quad_ib_data, VEER_STATIC_ARRAY_SIZE(fullscreen_quad_ib_data)),
                "fullscreen_quad"
            );

            m_quad_mesh.m_index_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
            m_quad_mesh.m_vertex_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
        }

        VEER_LOG("build cube");
        {
            //     6--------7
            //    /|   _-  /|
            //   / |_-    / |
            //  4--+-----5  |
            //  |  2-----+--3
            //  | /  \   | /   z y
            //  |/     \ |/    |/
            //  0--------1     o--x
            static float cube_vb_data[] = {
                -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, // 0
                0.5f,  -0.5f, -0.5f, 0.0f, 0.5f, // 1
                -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f, // 2
                0.5f,  0.5f,  -0.5f, 0.5f, 0.0f, // 3
                -0.5f, -0.5f, 0.5f,  0.5f, 0.5f, // 4
                0.5f,  -0.5f, 0.5f,  0.5f, 1.0f, // 5
                -0.5f, 0.5f,  0.5f,  1.0f, 0.0f, // 6
                0.5f,  0.5f,  0.5f,  1.0f, 0.5f  // 7
            };
            static uint32_t cube_ib_data[] = {
                0, 1, 2, /**/ 2, 1, 3, // bottom face
                6, 7, 4, /**/ 4, 7, 5, // top face
                4, 5, 0, /**/ 0, 5, 1, // front face
                2, 3, 6, /**/ 6, 3, 7, // back face
                6, 4, 2, /**/ 2, 4, 0, // left face
                3, 1, 7, /**/ 7, 1, 5  // right face
            };

            m_cube_mesh = mesh_utils::build_mesh(
                *m_render_device,
                8u,
                veer::containers::span<float>(cube_vb_data, VEER_STATIC_ARRAY_SIZE(cube_vb_data)),
                veer::containers::span<uint32_t>(cube_ib_data, VEER_STATIC_ARRAY_SIZE(cube_ib_data)),
                "cube"
            );

            m_cube_mesh.m_index_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
            m_cube_mesh.m_vertex_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
        }

        VEER_LOG("build water");
        {
            static constexpr veer::math::vec2f s_mesh_size(1.f, 1.f);
            static constexpr veer::math::vec2u s_water_tile_dims(128u, 128u);

            static constexpr size_t s_index_buffer_size = (s_water_tile_dims.x() - 1u) * (s_water_tile_dims.y() - 1u) * 6u;
            static constexpr size_t s_vertex_buffer_size = s_water_tile_dims.x() * s_water_tile_dims.y() * 5u;

            veer::containers::resizable_array<uint32_t> index_buffer{};
            index_buffer.resize(s_index_buffer_size);
            veer::containers::resizable_array<float> vertex_buffer{};
            vertex_buffer.resize(s_vertex_buffer_size);

            // -1 because for N vertices, we only need N-1 squares
            for (size_t y = 0u; y < s_water_tile_dims.y() - 1; ++y)
            {
                for (size_t x = 0u; x < s_water_tile_dims.x() - 1; ++x)
                {
                    //    ...   ...
                    // 2X---2X+1--2X+2-
                    //  |     |     |  ...
                    //  X----X+1---X+2-
                    //  |  \  |  \  |  ...
                    //  0-----1-----2--

                    // x + X (y+1)   (x+1) + X (y+1
                    //
                    // x + X (y)     (x+1) + X (y)

                    const size_t square_id = x + y * (s_water_tile_dims.x() - 1);
                    index_buffer[6u * square_id + 0u] = (x + 0) + (y + 0) * s_water_tile_dims.x();
                    index_buffer[6u * square_id + 1u] = (x + 0) + (y + 1) * s_water_tile_dims.x();
                    index_buffer[6u * square_id + 2u] = (x + 1) + (y + 0) * s_water_tile_dims.x();
                    index_buffer[6u * square_id + 3u] = (x + 1) + (y + 0) * s_water_tile_dims.x();
                    index_buffer[6u * square_id + 4u] = (x + 0) + (y + 1) * s_water_tile_dims.x();
                    index_buffer[6u * square_id + 5u] = (x + 1) + (y + 1) * s_water_tile_dims.x();
                }
            }

            for (size_t y = 0u; y < s_water_tile_dims.y(); ++y)
            {
                for (size_t x = 0u; x < s_water_tile_dims.x(); ++x)
                {
                    veer::math::vec2f uv(static_cast<float>(x), static_cast<float>(y));
                    uv /= (veer::math::vec2f(s_water_tile_dims) - veer::math::vec2f(1.f));
                    uv = veer::math::clamp(uv, veer::math::vec2f(0.f), veer::math::vec2f(1.f));

                    veer::math::vec2f pos_xy = veer::math::lerp(-0.5f * s_mesh_size, 0.5f * s_mesh_size, uv);

                    const size_t vertex_id = x + y * (s_water_tile_dims.x());
                    vertex_buffer[5u * vertex_id + 0u] = pos_xy.x();
                    vertex_buffer[5u * vertex_id + 1u] = pos_xy.y();
                    vertex_buffer[5u * vertex_id + 2u] = 0.f;
                    vertex_buffer[5u * vertex_id + 3u] = uv.x();
                    vertex_buffer[5u * vertex_id + 4u] = uv.y();
                }
            }

            m_water_mesh = mesh_utils::build_mesh(*m_render_device, s_water_tile_dims.x() * s_water_tile_dims.y(), vertex_buffer, index_buffer, "water");

            m_water_mesh.m_index_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
            m_water_mesh.m_vertex_buffer->upload(vdr::render_device_resource::upload_flags::dirty_all);
        }
    }

    application::~application()
    {
        m_swap_chain.reset();

        m_blit_pass = {};
        m_placeholder_pass = {};
        m_water_pass = {};

        m_quad_mesh = {};
        m_cube_mesh = {};
        m_water_mesh = {};

        m_fft_renderer.reset();
        m_waves_renderer.reset();

        m_frame_resource_context = {};

        m_window_service.reset();
        m_render_thread.reset();
        m_render_device.reset();
    }

    void on_window_dimensions_changed(veer::math::vec2u _dims)
    {
        VEER_LOG("Window dimensions : ( " << _dims[0] << ", " << _dims[1] << ")");
    }

    void application::init_geometry()
    {
        VEER_LOG("application::init_geometry");
        build_geometry();
    }

    void application::update_render_targets(vdw::window& _window)
    {
        VEER_LOG("application::update_render_targets");
        vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();

        // rebuild and clear render targets
        // Doing this first since we need to wait after this that the upload is done
        // But we also need to make sur the backbuffer is not used anymore. Thus we can combine both wait operations if we
        // rebuild render targets before rebuilding swap chain
        {
            // The desc of the backbuffer is not yet updated, so we just make up one that will be the same as the new one
            vdr::texture_2d_desc backbuffer_desc = m_swap_chain->get_current_backbuffer().get_desc();
            backbuffer_desc.m_size = _window.get_size();

            vdr::graphics_command_buffer upload_render_targets_buffer(*m_render_thread);

            // main render resources update
            m_frame_resource_context.update(*m_render_device, upload_render_targets_buffer, backbuffer_desc);

            graphics_command_queue.enqueue(std::move(upload_render_targets_buffer));
        }

        // Wait for all previous work
        graphics_command_queue.flush();

        // rebuild swap chain and backbuffer
        m_swap_chain.reset(); // destroy old swap_chain
        m_swap_chain = m_render_device->alloc<vdr::swap_chain>(_window);

        m_fence_value++;

        // setting all fence values as last backbuffer fence value (highest one)
        for (size_t i = 0; i < vdr::swap_chain::s_swap_chain_buffer_count; ++i)
            m_backbuffers_fence_values[i] = m_fence_value;

        graphics_command_queue.signal(m_fence_value);
        graphics_command_queue.wait_for_value(m_fence_value);
    }

    void application::update(vdw::window& _window, frame_context& _frame_context)
    {
        VEER_LOG("application::render");
        // scene :
        //   z y             C : camera
        //   |/     O
        //   0-x

        veer::math::vec2f window_size = _window.get_size();
        m_camera.m_transform =
            veer::math::transform(veer::math::vec3f(0.f, -25.f, 25.f), static_cast<float>(veer::math::s_pi) * veer::math::vec3f(0.25f, 0.f, 0.f), veer::math::vec3f(1.f));
        m_camera.m_projection = veer::math::projection::s_perspective(static_cast<float>(veer::math::s_pi) * 0.5f, window_size.x() / window_size.y(), 0.05f, 100.f);
        // m_camera.m_projection = veer::math::projection::s_orthographic(veer::math::vec3f(-10.f, -10.f, 0.1f), veer::math::vec3f(10.f, 10.f, 1000.f));

        m_cube_transform = veer::math::transform(
            veer::math::vec3f(0.f, 0.f, 0.5f),
            static_cast<float>(veer::math::s_pi) * veer::math::vec3f(0.f, 0.f, _frame_context.m_time),
            veer::math::vec3f(1.f)
        );

        m_water_transform = veer::math::transform(veer::math::vec3f(0.f, 0.f, 0.5f), veer::math::vec3f(0.f), veer::math::vec3f(128.f, 128.f, 1.f));

        m_floor_transform = veer::math::transform(veer::math::vec3f(0.f), veer::math::vec3f(0.f), veer::math::vec3f(5.f, 5.f, 1.f));
    }

    void application::render(vdw::window& _window, frame_context& _frame_ctx)
    {
        VEER_LOG("application::render");
        vdr::render_device_texture_2d& backbuffer_resource = m_swap_chain->get_current_backbuffer();
        uint64_t backbuffer_index = m_swap_chain->get_backbuffer_index();

        // fill frame Constant buffers
        {
            vdr::constant_buffer::update_context cb_update_context(*m_frame_resource_context.m_frame_constant_buffer, *m_render_thread);

            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_camera_view, _frame_ctx.m_camera.m_transform.generate_inverse_matrix());
            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_camera_proj, _frame_ctx.m_camera.m_projection);
            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_dt, _frame_ctx.m_delta_time);
            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_time, _frame_ctx.m_time);
            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_mouse_position, _window.get_mouse_position());
            cb_update_context.set_constant(frame_constant_buffer_elem_infos::s_window_size, _window.get_size());
        }

        // Clear RT
        // math::vec4f clear_color( 0.4f, 0.6f, 0.9f, 1.0f );
        // m_frame_command_buffer->clear_render_target(backbuffer_resource, clear_color);

        // Draw !!
        vdr::viewport viewport{.m_position = veer::math::vec2u(0u, 0u), .m_size = _window.get_size(), .m_depth = veer::math::vec2f(0.f, 1.f)};

        vdr::rect scissor_rect{.m_min = veer::math::vec2u(0u, 0u), .m_max = viewport.m_size};

        {
            vdr::graphics_command_buffer clear_command_buffer(*m_render_thread);

            clear_command_buffer.transition_barrier(backbuffer_resource, vdr::render_device_resource_sync_state::RenderTarget);
            clear_command_buffer.clear_render_target(backbuffer_resource, veer::math::vec4f(0.f, 0.f, 0.2f, 0.f));

            clear_command_buffer.transition_barrier(*m_frame_resource_context.m_depth, vdr::render_device_resource_sync_state::DepthWrite);
            clear_command_buffer.clear_depth_stencil(*m_frame_resource_context.m_depth, 0.f, 0u);

            vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();
            graphics_command_queue.enqueue(std::move(clear_command_buffer));
        }

        {
            m_waves_renderer->init_spectrum(_frame_ctx, *m_render_thread);
            m_waves_renderer->animate_spectrum(_frame_ctx, *m_render_thread);
        }

        {
            m_fft_renderer->compute_fft_512_512(
                _frame_ctx,
                *m_render_thread,
                *m_frame_resource_context.m_displacement_spectrum_render_target,
                *m_frame_resource_context.m_displacement_render_target
            );
            m_fft_renderer->compute_fft_512_512(
                _frame_ctx,
                *m_render_thread,
                *m_frame_resource_context.m_derivatives_spectrum_render_target,
                *m_frame_resource_context.m_derivatives_render_target
            );
        }

        // meshes
        struct drawing_mesh
        {
            const mesh& m_mesh;
            const veer::math::transform& m_transform;
            veer::math::vec3f m_albedo;
        };

        drawing_mesh meshes_to_draw[] = {
            {m_cube_mesh, m_cube_transform,  veer::math::vec3f(0.8f, 0.9f, 0.8f)},
            {m_quad_mesh, m_floor_transform, veer::math::vec3f(0.8f, 0.8f, 0.8f)},
        };

        for (drawing_mesh& mesh_to_draw : meshes_to_draw)
        {
            vdr::graphics_command_buffer mesh_rendering_command_buffer(*m_render_thread);
            vdr::graphics_submit_context gfx_ctx(mesh_rendering_command_buffer, *m_placeholder_pass.m_technique);

            gfx_ctx.set_render_output(m_frame_resource_context.m_depth.get(), &backbuffer_resource);
            gfx_ctx.set_viewports(veer::containers::span<vdr::viewport>(viewport));
            gfx_ctx.set_mesh(*mesh_to_draw.m_mesh.m_index_buffer, *mesh_to_draw.m_mesh.m_vertex_buffer);

            {
                vdr::constant_buffer::update_context cb_update_context(*m_placeholder_pass.m_material_constant_buffer, *m_render_thread);

                cb_update_context.set_constant(m_placeholder_pass.m_params_id.m_model, mesh_to_draw.m_transform.generate_matrix());
                cb_update_context.set_constant(m_placeholder_pass.m_params_id.m_albedo, mesh_to_draw.m_albedo);
            }

            gfx_ctx.set_constant_buffer(*m_frame_resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
            gfx_ctx.set_constant_buffer(*m_placeholder_pass.m_material_constant_buffer, vdr::constant_buffer_type::material);

            gfx_ctx.draw_indexed_instanced(mesh_to_draw.m_mesh.m_index_buffer->get_desc().m_size, 1u);

            vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();
            graphics_command_queue.enqueue(std::move(mesh_rendering_command_buffer));
        }

        {
            vdr::graphics_command_buffer water_rendering_command_buffer(*m_render_thread);
            vdr::graphics_submit_context gfx_ctx(water_rendering_command_buffer, *m_water_pass.m_technique);

            gfx_ctx.set_render_output(m_frame_resource_context.m_depth.get(), &backbuffer_resource);
            gfx_ctx.set_viewports(veer::containers::span<vdr::viewport>(viewport));
            gfx_ctx.set_mesh(*m_water_mesh.m_index_buffer, *m_water_mesh.m_vertex_buffer);

            {
                vdr::constant_buffer::update_context cb_update_context(*m_water_pass.m_material_constant_buffer, *m_render_thread);

                cb_update_context.set_constant(m_water_pass.m_params_id.m_model, m_water_transform.generate_matrix());
                cb_update_context.set_constant(m_water_pass.m_params_id.m_lambda, veer::math::vec3f(1.f, 1.f, 1.f));
                cb_update_context.set_constant(m_water_pass.m_params_id.m_patch_length, m_waves_renderer->get_patch_length());
                cb_update_context.set_texture_read_only(m_water_pass.m_params_id.m_waves_displacement_tex, *m_frame_resource_context.m_displacement_render_target);
                cb_update_context.set_texture_read_only(m_water_pass.m_params_id.m_waves_derivatives_tex, *m_frame_resource_context.m_derivatives_render_target);
            }

            gfx_ctx.set_constant_buffer(*m_frame_resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
            gfx_ctx.set_constant_buffer(*m_water_pass.m_material_constant_buffer, vdr::constant_buffer_type::material);

            gfx_ctx.draw_indexed_instanced(m_water_mesh.m_index_buffer->get_desc().m_size, 1u);

            vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();
            graphics_command_queue.enqueue(std::move(water_rendering_command_buffer));
        }

        if (false)
        {
            vdr::graphics_command_buffer blit_command_buffer(*m_render_thread);
            vdr::graphics_submit_context gfx_ctx(blit_command_buffer, *m_blit_pass.m_technique);

            gfx_ctx.set_render_output(nullptr, &backbuffer_resource);
            gfx_ctx.set_viewports(veer::containers::span<vdr::viewport>(viewport));
            gfx_ctx.set_mesh(*m_quad_mesh.m_index_buffer, *m_quad_mesh.m_vertex_buffer);

            {
                vdr::constant_buffer::update_context cb_update_context(*m_blit_pass.m_material_constant_buffer, *m_render_thread);

                cb_update_context.set_constant(m_blit_pass.m_params_id.m_viewport_size, viewport.m_size);
                cb_update_context.set_texture_read_only(m_blit_pass.m_params_id.m_input_tex, *m_frame_resource_context.m_displacement_render_target);
            }

            gfx_ctx.set_constant_buffer(*m_frame_resource_context.m_frame_constant_buffer, vdr::constant_buffer_type::frame);
            gfx_ctx.set_constant_buffer(*m_blit_pass.m_material_constant_buffer, vdr::constant_buffer_type::material);

            gfx_ctx.draw_indexed_instanced(m_quad_mesh.m_index_buffer->get_desc().m_size, 1u);

            vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();
            graphics_command_queue.enqueue(std::move(blit_command_buffer));
        }

        {
            vdr::graphics_command_buffer present_command_buffer(*m_render_thread);

            // Transition RT to Present state (needed for Present call)
            present_command_buffer.transition_barrier(backbuffer_resource, vdr::render_device_resource_sync_state::Present);

            vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();
            graphics_command_queue.enqueue(std::move(present_command_buffer));
        }
    }

    int application::run()
    {
        vdr::command_queue& graphics_command_queue = m_render_device->get_graphics_command_queue();

        m_t0_global = m_clock.now();
        m_t0_frame = m_t0_global;

        veer::math::vec2u current_win_size(0u, 0u);

        vdw::window& window = m_window_service->get_window(0u);
        size_t backbuffer_index = m_swap_chain->get_backbuffer_index();

        bool force_update_render_targets = true;
        bool need_init_geometry = true;

        // TODO create event handler
        while (window.is_open())
        {
            window.poll_events();

            auto tmp = m_clock.now();
            std::chrono::duration<float> dt = tmp - m_t0_frame;
            std::chrono::duration<float> time = tmp - m_t0_global;
            m_t0_frame = tmp;
            m_frame++;

            float fps = dt.count() > 0.f ? 1.f / dt.count() : -1.f;
            if (!(m_frame & 0xF)) // once every 16 frames
                VEER_LOG_DEBUG("dt : " << dt.count() << " | fps : " << fps);

            vdr::render_device_texture_2d& backbuffer_resource = m_swap_chain->get_current_backbuffer();
            m_render_thread->start_frame(m_frame);

            if (need_init_geometry)
            {
                init_geometry();
                need_init_geometry = false;
            }
            else if (force_update_render_targets || any(window.get_size() != current_win_size))
            {
                VEER_LOG("Window size changed : " << window.get_size());
                // update render targets
                update_render_targets(window);
                current_win_size = window.get_size();
                force_update_render_targets = false;
            }
            else
            {
                if (all(current_win_size == veer::math::vec2u(0u, 0u)))
                    continue;

                frame_context frame_context{
                    .m_device = *m_render_device,
                    .m_resource_context = m_frame_resource_context,
                    .m_backbuffer_desc = backbuffer_resource.get_desc(),
                    .m_camera = m_camera,
                    .m_frame = m_frame,
                    .m_time = time.count(),
                    .m_delta_time = dt.count(),
                };

                update(window, frame_context);

                render(window, frame_context);
            }

            graphics_command_queue.flush();
            m_swap_chain->present(0);

            {
                // Signal current back buffer usage is ended
                m_fence_value++;
                // m_backbuffers_fence_values[backbuffer_index] = m_fence_value;
                graphics_command_queue.signal(m_fence_value);

                // backbuffer_index = m_swap_chain->get_backbuffer_index();

                // Wait for next frame back buffer end of next frame back buffer
                // graphics_command_queue.wait_for_value(m_backbuffers_fence_values[backbuffer_index]);
                graphics_command_queue.wait_for_value(m_fence_value);
            }

            m_render_thread->end_frame();
        }

        // Wait for all previous work
        m_fence_value++;
        graphics_command_queue.signal(m_fence_value);
        graphics_command_queue.wait_for_value(m_fence_value);

        return 0;
    }

#if defined(_DEBUG)
    veer::tests::result run_all_tests()
    {
        VEER_RUN_TEST(veer::tests::vec_all());
        VEER_RUN_TEST(veer::tests::mat_all());
        VEER_RUN_TEST(veer::tests::transform_all());
        return veer::tests::result::succeeded;
    }
#endif // defined(_DEBUG)
} // namespace te