module frame_constant_buffer;

#include "constant_buffer.h"

public struct frame_data
{
    // TODO : handle multiple viewports ?
    public float4x4 camera_view;
    public float4x4 camera_proj;
    public float time;             // time since start of app
    public float dt;               // last frame delta time
    public float2 mouse_position;   // mouse position in pixels
    public uint2 window_size;       // current frame window size in pixels
    // pad 2*4
};

public DECLARE_FRAME_CONSTANT_BUFFER(frame_data, g_frame_cb);