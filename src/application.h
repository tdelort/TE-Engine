#pragma once

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

    class application
    {
        using descriptor_handle_t = veer::math::vec<vdr::render_device_resource::bindless_id, 2u>;

    public:
        static constexpr const char* s_precompiled_vs_blit_path = "vs_blit.bin";
        static constexpr const char* s_precompiled_ps_blit_path = "ps_blit.bin";
        static constexpr const char* s_precompiled_vs_placeholder_path = "vs_placeholder.bin";
        static constexpr const char* s_precompiled_ps_placeholder_path = "ps_placeholder.bin";
        static constexpr const char* s_precompiled_vs_water_path = "vs_water.bin";
        static constexpr const char* s_precompiled_ps_water_path = "ps_water.bin";

    public:
        application();
        ~application();

    public:
        int run();

    private:
        void create_window();
        void create_swap_chain();
        void build_shader_techniques();
        void build_geometry();

    private:
        void init_geometry();
        void update_render_targets(vdw::window& _window);

        void update(vdw::window& _window, frame_context& _frame_ctx);
        void render(vdw::window& _window, frame_context& _frame_ctx);

    private:
        veer::unique_ptr<vdw::windowing_service> m_window_service{nullptr};
        veer::unique_ptr<vdr::render_device> m_render_device{nullptr};
        veer::unique_ptr<vdr::render_thread> m_render_thread{nullptr};

        veer::unique_ptr<vdr::swap_chain> m_swap_chain{nullptr};

        uint64_t m_frame{0u};
        uint64_t m_fence_value{0u};
        uint64_t m_backbuffers_fence_values[vdr::swap_chain::s_swap_chain_buffer_count] = {};

        // implementation details on m_time, we don't care a lot since it is in the TODO list of things to reimplement
        std::chrono::high_resolution_clock m_clock;
        std::chrono::high_resolution_clock::time_point m_t0_global;
        std::chrono::high_resolution_clock::time_point m_t0_frame;

        camera m_camera{};

        mesh m_quad_mesh{};
        mesh m_cube_mesh{};
        mesh m_water_mesh{};

        veer::math::transform m_cube_transform{};
        veer::math::transform m_floor_transform{};
        veer::math::transform m_water_transform{};

        struct blit_params
        {
            vdr::buffer_elem_info m_viewport_size;
            vdr::buffer_elem_info m_input_tex;
        };
        shader_utils::graphics_pass<blit_params> m_blit_pass;

        struct placeholder_params
        {
            vdr::buffer_elem_info m_model;
            vdr::buffer_elem_info m_albedo;
        };
        shader_utils::graphics_pass<placeholder_params> m_placeholder_pass;

        struct water_params
        {
            vdr::buffer_elem_info m_model;
            vdr::buffer_elem_info m_lambda;
            vdr::buffer_elem_info m_patch_length;
            vdr::buffer_elem_info m_waves_displacement_tex;
            vdr::buffer_elem_info m_waves_derivatives_tex;
        };
        shader_utils::graphics_pass<water_params> m_water_pass;

        frame_render_resource_context m_frame_resource_context;

        veer::unique_ptr<fft_renderer> m_fft_renderer{nullptr};
        veer::unique_ptr<waves_renderer> m_waves_renderer{nullptr};
    };
} // namespace te