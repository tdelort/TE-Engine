#pragma once

#include "frame_render_resource_context.h"
#include "camera.h"

#include <veer/core/containers/span.h>
#include <veer/core/math/mat.h>
#include <veer/core/math/transform.h>

#include <veer/display/render/render_device.h>
#include <veer/display/render/resource_desc.h>

namespace te
{
    struct frame_context
    {
        veer::display::render::render_device& m_device;
        frame_render_resource_context& m_resource_context;
        veer::display::render::texture_2d_desc m_backbuffer_desc;
        const camera& m_camera;
        uint64_t m_frame{0u};
        float m_time{0.f};
        float m_delta_time{0.f};
    };

} // namespace te