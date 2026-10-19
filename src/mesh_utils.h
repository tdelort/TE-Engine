#pragma once

#include <veer/core/unique_ptr.h>
#include <veer/display/render/render_device.h>
#include <veer/display/render/render_device_buffer.h>

struct mesh
{
    veer::unique_ptr<veer::display::render::render_device_buffer> m_index_buffer{};
    veer::unique_ptr<veer::display::render::render_device_buffer> m_vertex_buffer{};
};

namespace mesh_utils
{
    mesh build_mesh(
        veer::display::render::render_device& _device,
        size_t _vertex_count,
        veer::containers::span<float> _vertex_buffer,
        veer::containers::span<uint32_t> _index_buffer,
        const char* _debug_name
    );
};