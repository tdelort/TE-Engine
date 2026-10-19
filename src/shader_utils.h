#pragma once

#include <veer/core/unique_ptr.h>
#include <veer/display/render/compute_technique.h>
#include <veer/display/render/constant_buffer.h>
#include <veer/display/render/graphics_technique.h>
#include <veer/display/render/shader_source.h>

namespace shader_utils
{
#if defined(D3D12_RENDER_BACKEND)
    static constexpr const char* s_shader_assets_path = "..\\..\\data\\shaders\\dx12\\";
#endif // defined(D3D12_RENDER_BACKEND)

    template <typename TECHNIQUE, typename PARAMS>
        requires std::is_base_of_v<veer::display::render::technique, TECHNIQUE>
    struct pass
    {
        veer::unique_ptr<TECHNIQUE> m_technique{nullptr};
        veer::unique_ptr<veer::display::render::constant_buffer> m_material_constant_buffer{nullptr};
        PARAMS m_params_id{};
    };

    template <typename PARAMS>
    using graphics_pass = pass<veer::display::render::graphics_technique, PARAMS>;

    template <typename PARAMS>
    using compute_pass = pass<veer::display::render::compute_technique, PARAMS>;

    veer::display::render::shader_code_memory_blob_t read_shader_from_disk(const char* _shader_name);

    veer::unique_ptr<veer::display::render::compute_technique> build_compute_shader_from_disk(
        const char* _shader_name,
        const veer::display::render::render_device& _device,
        veer::display::render::constant_buffer_definition& _frame_constant_buffer_definition,
        veer::display::render::constant_buffer_definition& _material_constant_buffer_definition
    );
} // namespace shader_utils

// ONLY TO BE USED AFTER THE CONSTANT BUFFER IS SET !!!
#define VEER_FILL_PASS_PARAM_ID_HELPER(_pass, _field) (_pass).m_params_id.m_##_field = (_pass).m_material_constant_buffer->get_def().get_elem_info(#_field);