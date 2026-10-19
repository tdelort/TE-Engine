#include <veer/core/math/mat.h>
#include <veer/core/math/transform.h>

namespace te
{
    using namespace veer;

    struct camera
    {
        veer::math::mat4x4f m_projection{veer::math::mat4x4f::identity()};
        veer::math::transform m_transform{};
    };
}