#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

//!< 16 View * 5 Draw call
#define TILE_DIMENSION_MAX (16 * 5)

layout (location = 0) in vec2 InTexcoord[];

layout (set = 0, binding = 0) uniform VIEW_PROJECTION_BUFFER
{
	mat4 ViewProjection[TILE_DIMENSION_MAX];
} VPB;

layout (location = 0) out vec2 OutTexcoord;

layout (triangles, invocations = 16) in;
layout (triangle_strip, max_vertices = 3) out;
void main()
{
	for(int i=0;i<gl_in.length();++i) {
		gl_Position = VPB.ViewProjection[gl_InvocationID] * gl_in[i].gl_Position;	
		OutTexcoord = InTexcoord[i];
		gl_ViewportIndex = gl_InvocationID;
		gl_PointSize = 5.0f;

		EmitVertex();
	}
	EndPrimitive();	
}
