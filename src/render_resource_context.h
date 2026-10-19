#pragma once

#include <veer/core/containers/resizable_array.h>
#include <veer/core/unique_ptr.h>
#include <veer/display/render/constant_buffer_definition.h>
#include <veer/display/render/resource_desc.h>

namespace veer::display::render
{
    class render_device;
    class render_device_resource;
    class constant_buffer;
    class render_device_buffer;
    class render_device_texture_2d;
    class render_device_texture_3d;
    class graphics_command_buffer;
} // namespace veer::display::render
namespace vdr = veer::display::render;

class render_resource_context
{
public:
    template <typename RESOURCE_TYPE, typename DESC_TYPE>
    struct basic_resource_config
    {
        enum class flags
        {
            none = 0,
            force_rebuild = 1 << 0,
            clear = 1 << 1,
        };

        basic_resource_config()
        {
        }

        basic_resource_config(
            DESC_TYPE&& _desc, veer::unique_ptr<RESOURCE_TYPE>& _resource, const char* _debug_name,
            flags _flags = flags::none, veer::math::vec4f _clear_value = veer::math::vec4f(0.f)
        )
            : m_resource(&_resource)
            , m_desc(std::move(_desc))
            , m_flags(_flags)
            , m_clear_value(_clear_value)
        {
        }

        veer::unique_ptr<RESOURCE_TYPE>* m_resource{nullptr};
        DESC_TYPE m_desc{};
        const char* m_debug_name{nullptr};
        flags m_flags{flags::none};
        veer::math::vec4f m_clear_value{0.f};
    };

    using buffer_config = basic_resource_config<vdr::render_device_buffer, vdr::buffer_desc>;
    using texture_2d_config = basic_resource_config<vdr::render_device_texture_2d, vdr::texture_2d_desc>;
    using texture_3d_config = basic_resource_config<vdr::render_device_texture_3d, vdr::texture_3d_desc>;

    struct constant_buffer_config
    {
        enum class flags
        {
            none = 0,
            force_rebuild = 1 << 0,
        };

        constant_buffer_config()
        {
        }

        constant_buffer_config(
            const vdr::constant_buffer_definition& _def, veer::unique_ptr<vdr::constant_buffer>& _resource,
            const char* _debug_name, flags _flags = flags::none
        )
            : m_resource(&_resource)
            , m_definition(&_def)
            , m_debug_name(_debug_name)
            , m_flags(_flags)
        {
        }
        const vdr::constant_buffer_definition* m_definition{nullptr};
        veer::unique_ptr<vdr::constant_buffer>* m_resource{nullptr};
        const char* m_debug_name{nullptr};
        flags m_flags{flags::none};
    };

protected:
    void update(vdr::render_device& _device, vdr::graphics_command_buffer& _command_buffer);

protected:
    veer::containers::resizable_array<constant_buffer_config> m_constant_buffer_config;
    veer::containers::resizable_array<buffer_config> m_buffer_config;
    veer::containers::resizable_array<texture_2d_config> m_texture_2d_config;
    veer::containers::resizable_array<texture_3d_config> m_texture_3d_config;
};

VEER_ENUM_CLASS_FLAG_OPERATORS(render_resource_context::buffer_config::flags);
VEER_ENUM_CLASS_FLAG_OPERATORS(render_resource_context::texture_2d_config::flags);
VEER_ENUM_CLASS_FLAG_OPERATORS(render_resource_context::texture_3d_config::flags);
VEER_ENUM_CLASS_FLAG_OPERATORS(render_resource_context::constant_buffer_config::flags);