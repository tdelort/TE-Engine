#include "shader_utils.h"

#include <veer/core/containers/string.h>
#include <veer/core/debug.h>
#include <veer/display/render/shader_source.h>

#include <filesystem>
#include <fstream>

using namespace veer;

namespace vdr = veer::display::render;

namespace shader_utils
{
    display::render::shader_code_memory_blob_t read_shader_from_disk(const char* _shader_name)
    {
        VEER_LOG("Reading shader " << _shader_name << " from disk");
        // TODO : implement path class
        containers::string full_path;
        full_path.append(std::filesystem::current_path().c_str());
        full_path.append("/");
        full_path.append(s_shader_assets_path);
        full_path.append(_shader_name);

        VEER_LOG("Shader full path " << full_path.c_str());

        display::render::shader_code_memory_blob_t blob;

        try
        {

            const size_t size_in_bytes = std::filesystem::file_size(full_path.c_str());

            VEER_LOG("Shader file size " << size_in_bytes);

            blob.resize(size_in_bytes);

            std::basic_ifstream<byte_t> fs;
            fs.open(full_path.c_str(), std::ios::in | std::ios::binary);

            VEER_ASSERT(fs.is_open(), "Failed to open file " << full_path.c_str());

            fs.read(blob.data(), size_in_bytes);

            VEER_ASSERT(!fs.fail(), "Error reading file " << full_path.c_str());

            fs.close();
        }
        catch (std::exception e)
        {
            VEER_LOG_ERROR("Exception while reading shader from disk " << e.what());
        }

        return blob;
    }

    veer::unique_ptr<veer::display::render::compute_technique> build_compute_shader_from_disk(
        const char* _shader_name, const veer::display::render::render_device& _device,
        veer::display::render::constant_buffer_definition& _frame_constant_buffer_definition,
        veer::display::render::constant_buffer_definition& _material_constant_buffer_definition
    )
    {
        vdr::shader_code_memory_blob_t cs_blob = shader_utils::read_shader_from_disk(_shader_name);

        vdr::shader_stage_source_container_t source_code = {};
        source_code[static_cast<size_t>(vdr::shader_stage_type::compute)] = cs_blob;

        return veer::unique_ptr<vdr::compute_technique>::make(
            _frame_constant_buffer_definition, _material_constant_buffer_definition, _device,
            source_code
        );
    }
} // namespace shader_utils