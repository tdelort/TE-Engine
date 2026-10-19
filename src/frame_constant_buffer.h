#pragma once

#include <veer/core/containers/static_array.h>
#include <veer/display/render/constant_buffer_definition.h>

namespace vdr = veer::display::render;

// TODO : This one can probably stay as static data here. I could retrieve it from reflection but it will not change a
// lot (this statement will become true one day) compared to material constant buffers
namespace frame_constant_buffer_elem_infos
{
    static constexpr vdr::buffer_elem_info s_camera_view{0u, 64u};
    static constexpr vdr::buffer_elem_info s_camera_proj{64u, 64u};

    static constexpr vdr::buffer_elem_info s_time{128u, 4u};
    static constexpr vdr::buffer_elem_info s_dt{132u, 4u};
    static constexpr vdr::buffer_elem_info s_mouse_position{136u, 8u};

    static constexpr vdr::buffer_elem_info s_window_size{144u, 8u};
}; // namespace frame_constant_buffer_elem_infos

static constexpr veer::containers::static_array<vdr::constant_buffer_elem_info, 6> s_frame_constant_buffer_elems = {
    vdr::constant_buffer_elem_info{"camera_view",    frame_constant_buffer_elem_infos::s_camera_view   },
    vdr::constant_buffer_elem_info{"camera_proj",    frame_constant_buffer_elem_infos::s_camera_proj   },
    vdr::constant_buffer_elem_info{"time",           frame_constant_buffer_elem_infos::s_time          },
    vdr::constant_buffer_elem_info{"dt",             frame_constant_buffer_elem_infos::s_dt            },
    vdr::constant_buffer_elem_info{"mouse_position", frame_constant_buffer_elem_infos::s_mouse_position},
    vdr::constant_buffer_elem_info{"window_size",    frame_constant_buffer_elem_infos::s_window_size   },
};
