#pragma once

#define DECLARE_FRAME_CONSTANT_BUFFER(_type, _name) ConstantBuffer<_type> _name : register(b0)
#define DECLARE_MATERIAL_CONSTANT_BUFFER(_type, _name) ConstantBuffer<_type> _name : register(b1)