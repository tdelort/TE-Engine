#pragma once

#include "frame_constant_buffer.h"
#include "render_resource_context.h"

// TODO : :(
#include <random>

#include <veer/display/render/command_buffer.h>
#include <veer/display/render/constant_buffer.h>
#include <veer/display/render/render_device.h>
#include <veer/display/render/render_device_texture_2d.h>

namespace vdr = veer::display::render;

class frame_render_resource_context : public render_resource_context
{
public:
    veer::unique_ptr<vdr::render_device_texture_2d> m_depth;
    veer::unique_ptr<vdr::render_device_texture_2d> m_spectrum_t0_render_target;
    veer::unique_ptr<vdr::render_device_texture_2d> m_displacement_spectrum_render_target;
    veer::unique_ptr<vdr::render_device_texture_2d> m_derivatives_spectrum_render_target;
    veer::unique_ptr<vdr::render_device_texture_2d> m_displacement_render_target;
    veer::unique_ptr<vdr::render_device_texture_2d> m_derivatives_render_target;
    veer::unique_ptr<vdr::render_device_texture_2d> m_gaussian_noise_tex;
    veer::unique_ptr<vdr::constant_buffer> m_frame_constant_buffer;

public:
    // https://en.wikipedia.org/wiki/Box-Muller_transform#Basic_form
    std::pair<float, float> s_random_normal_box_muller(float _mean, float _std_dev, std::mt19937& _rng)
    {
        constexpr float epsilon = FLT_EPSILON;

        std::uniform_real_distribution<float> u_distribution(epsilon, 1.f);
        const float u = u_distribution(_rng);

        std::uniform_real_distribution<float> v_distribution(0.f, 1.f);
        const float v = v_distribution(_rng);

        const float common_factor = _std_dev * std::sqrt(-2.f * std::log(u));

        const float cos_2pi_v = static_cast<float>(std::cos(2.f * veer::math::s_pi * v));
        const float sin_2pi_v = static_cast<float>(std::sin(2.f * veer::math::s_pi * v));

        const float x = common_factor * cos_2pi_v + _mean;
        const float y = common_factor * sin_2pi_v + _mean;

        return std::make_pair(x, y);
    }

    void s_fill_gaussian_noise_texture(float _mean, float _std_dev, vdr::render_device_texture_2d& _texture)
    {
        std::mt19937 rng;

        const vdr::texture_2d_desc& desc = _texture.get_desc();

        const size_t channel_count = 2;
        // bytes array, r16g16 -> 2 bytes per channel
        veer::containers::resizable_array<uint16_t> data;
        data.resize(desc.m_size.x() * desc.m_size.y() * channel_count);

        for (unsigned int y = 0; y < desc.m_size.y(); ++y)
        {
            for (unsigned int x = 0; x < desc.m_size.x(); ++x)
            {
                std::pair<float, float> rg = s_random_normal_box_muller(_mean, _std_dev, rng);
                data[(y * desc.m_size.x() + x) * channel_count + 0] = veer::math::to_float_16(rg.first);
                data[(y * desc.m_size.x() + x) * channel_count + 1] = veer::math::to_float_16(rg.second);
            }
        }

        _texture.set_data<uint16_t>(veer::containers::span<uint16_t>(data.data(), data.size()));
    }

    void update(vdr::render_device& _device, vdr::graphics_command_buffer& _command_buffer, vdr::texture_2d_desc& _backbuffer_desc)
    {
        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = _backbuffer_desc.m_size,
                .m_format = vdr::render_device_data_format::d32_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::depth_stencil,
            },
            m_depth,
            "depth_render_target",
            texture_2d_config::flags::clear,
            veer::math::vec4f(0.f)
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::storage,
            },
            m_spectrum_t0_render_target,
            "spectrum_t0_render_target"
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16b16a16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::storage,
            },
            m_displacement_spectrum_render_target,
            "displacement_spectrum_render_target"
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16b16a16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::storage,
            },
            m_derivatives_spectrum_render_target,
            "derivatives_spectrum_render_target"
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16b16a16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::storage,
            },
            m_displacement_render_target,
            "displacement_render_target"
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16b16a16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::storage,
            },
            m_derivatives_render_target,
            "derivatives_render_target"
        );

        m_texture_2d_config.emplace_back(
            vdr::texture_2d_desc{
                .m_size = vdr::texture_2d_desc::size_type(512u, 512u),
                .m_format = vdr::render_device_data_format::r16g16_float,
                .m_flags = vdr::texture_2d_desc::usage_flags::render_target,
            },
            m_gaussian_noise_tex,
            "gaussian_noise_tex"
        );

        vdr::constant_buffer_definition frame_constant_buffer_definition;
        frame_constant_buffer_definition.m_elements.add(s_frame_constant_buffer_elems.cbegin(), s_frame_constant_buffer_elems.cend());

        m_constant_buffer_config.emplace_back(frame_constant_buffer_definition, m_frame_constant_buffer, "frame_constant_buffer");

        render_resource_context::update(_device, _command_buffer);

        // TODO : make a texture wrapper and handle read/writing texture assets files
        // Requirements : abstracting read / write from OS, and probably adding an engine layer
        s_fill_gaussian_noise_texture(0.f, 1.f, *m_gaussian_noise_tex);

        m_gaussian_noise_tex->upload(_command_buffer, vdr::render_device_resource::upload_flags::dirty_data);
    }
};