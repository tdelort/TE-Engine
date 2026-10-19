#pragma once

#include "frame_context.h"
#include "render_resource_context.h"

namespace veer::display::render
{
    class render_thread;
    class graphics_command_buffer;
} // namespace veer::display::render

namespace vdr = veer::display::render;
namespace vdw = veer::display::window;

class render_node
{
public:
    render_node();

    virtual void render(frame_context& _ctx, vdr::render_thread& _render_thread) = 0;
};