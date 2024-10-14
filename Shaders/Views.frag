#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 InTexcoord;

layout (set = 0, binding = 1) uniform sampler2D ColorMap;

layout (location = 0) out vec4 Color;

layout (early_fragment_tests) in;
void main()
{
	Color = texture(ColorMap, InTexcoord);
}