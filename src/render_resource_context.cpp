#include "render_resource_context.h"

#include <veer/core/unique_ptr.h>
#include <veer/display/render/command_buffer.h>
#include <veer/display/render/render_device_buffer.h>
#include <veer/display/render/render_device_resource.h>
#include <veer/display/render/render_device_texture_2d.h>
#include <veer/display/render/render_device_texture_3d.h>
#include <veer/display/render/resource_desc.h>

void render_resource_context::update(vdr::render_device& _device, vdr::graphics_command_buffer& _command_buffer)
{
    for (buffer_config& config : m_buffer_config)
    {
        veer::unique_ptr<vdr::render_device_buffer>& resource = *(config.m_resource);
        if (resource == nullptr || veer::flags::get(config.m_flags, buffer_config::flags::force_rebuild))
        {
            resource = veer::unique_ptr<vdr::render_device_buffer>::make(_device, config.m_desc, config.m_debug_name);
        }
        else
        {
            resource->set_desc(config.m_desc);
        }

        resource->upload(vdr::render_device_resource::upload_flags::dirty_all);
    }

    for (texture_2d_config& config : m_texture_2d_config)
    {
        veer::unique_ptr<vdr::render_device_texture_2d>& resource = *(config.m_resource);
        if (resource == nullptr || veer::flags::get(config.m_flags, texture_2d_config::flags::force_rebuild))
        {
            resource = veer::unique_ptr<vdr::render_device_texture_2d>::make(_device, config.m_desc, config.m_debug_name);
        }
        else
        {
            resource->set_desc(config.m_desc);
        }

        resource->upload(_command_buffer, vdr::render_device_resource::upload_flags::dirty_all);
    }

    for (texture_3d_config& config : m_texture_3d_config)
    {
        veer::unique_ptr<vdr::render_device_texture_3d>& resource = *(config.m_resource);
        if (resource == nullptr || veer::flags::get(config.m_flags, texture_3d_config::flags::force_rebuild))
        {
            resource = veer::unique_ptr<vdr::render_device_texture_3d>::make(_device, config.m_desc, config.m_debug_name);
        }
        else
        {
            resource->set_desc(config.m_desc);
        }

        resource->upload(_command_buffer, vdr::render_device_resource::upload_flags::dirty_all);
    }

    for (constant_buffer_config& config : m_constant_buffer_config)
    {
        veer::unique_ptr<vdr::constant_buffer>& resource = *(config.m_resource);
        if (resource == nullptr || veer::flags::get(config.m_flags, constant_buffer_config::flags::force_rebuild))
        {
            resource = veer::unique_ptr<vdr::constant_buffer>::make(_device, *config.m_definition, config.m_debug_name);
        }
        else
        {
            VEER_ASSERT(*config.m_definition == resource->get_def(), "Definition changed between first update call and this one, constant buffer won't be updated !!");
        }
    }
    // Issuing all clear requests

    for (buffer_config& config : m_buffer_config)
    {
        if (!veer::flags::get(config.m_flags, buffer_config::flags::clear))
            continue;

        veer::unique_ptr<vdr::render_device_buffer>& resource = *(config.m_resource);
        VEER_LOG_ERROR("Not implemented");
    }

    for (texture_2d_config& config : m_texture_2d_config)
    {
        if (!veer::flags::get(config.m_flags, texture_2d_config::flags::clear))
            continue;

        veer::unique_ptr<vdr::render_device_texture_2d>& resource = *(config.m_resource);

        if (veer::flags::get(resource->get_desc().m_flags, vdr::texture_2d_desc::usage_flags::depth_stencil))
        {
            _command_buffer.transition_barrier(*resource, vdr::render_device_resource_sync_state::DepthWrite);
            _command_buffer.clear_depth_stencil(*resource, config.m_clear_value.x(), 0u);
        }
        else if (veer::flags::get(resource->get_desc().m_flags, vdr::texture_2d_desc::usage_flags::render_target))
        {
            _command_buffer.transition_barrier(*resource, vdr::render_device_resource_sync_state::RenderTarget);
            _command_buffer.clear_render_target(*resource, config.m_clear_value);
        }
    }

    for (texture_3d_config& config : m_texture_3d_config)
    {
        if (!veer::flags::get(config.m_flags, texture_3d_config::flags::clear))
            continue;

        veer::unique_ptr<vdr::render_device_texture_3d>& resource = *(config.m_resource);
        VEER_LOG_ERROR("Not implemented");
    }

    m_buffer_config.clear();
    m_texture_2d_config.clear();
    m_texture_3d_config.clear();
    m_constant_buffer_config.clear();
}
