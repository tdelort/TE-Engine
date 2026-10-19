#include "mesh_utils.h"

#include <veer/core/core.h>
#include <veer/display/render/render_device.h>
#include <veer/display/render/render_device_buffer.h>

namespace vdr = veer::display::render;

namespace mesh_utils
{
    mesh build_mesh(
        vdr::render_device& _device,
        size_t _vertex_count,
        veer::containers::span<float> _vertex_buffer,
        veer::containers::span<uint32_t> _index_buffer,
        const char* _debug_name
    )
    {
        const size_t vertex_size = _vertex_buffer.size() / _vertex_count;

        vdr::buffer_desc index_buffer_desc{
            .m_flags = vdr::buffer_desc::usage_flags::index,
            .m_size = _index_buffer.size(), // index_buffer_size indices ...
            .m_stride = sizeof(uint32_t)    // ... of uint32_t size
        };

        veer::containers::string index_buffer_name(_debug_name);
        index_buffer_name += "_index_buffer";

        vdr::buffer_desc vertex_buffer_desc{
            .m_flags = vdr::buffer_desc::usage_flags::vertex,
            .m_size = _vertex_buffer.size(),        // vertex_buffer_size vertices...
            .m_stride = vertex_size * sizeof(float) // ...containing vertex_size floats
        };

        veer::containers::string vertex_buffer_name(_debug_name);
        vertex_buffer_name += "_vertex_buffer";

        mesh result{
            .m_index_buffer = veer::unique_ptr<vdr::render_device_buffer>::make(_device, index_buffer_desc, index_buffer_name.c_str()),
            .m_vertex_buffer = veer::unique_ptr<vdr::render_device_buffer>::make(_device, vertex_buffer_desc, vertex_buffer_name.c_str())
        };

        result.m_index_buffer->set_data(_index_buffer);
        result.m_vertex_buffer->set_data(_vertex_buffer);
        return result;
    }
} // namespace mesh_utils