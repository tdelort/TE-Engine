#include "camera.h"

namespace te
{
    struct render_context
    {
        const camera& m_camera;
        uint64_t m_frame{0u};
        float m_time{0.f};
        float m_delta_time{0.f};
    };
}