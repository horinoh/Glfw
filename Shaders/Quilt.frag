#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (early_fragment_tests) in;

layout (location = 0) in vec2 InTexcoord;

layout (location = 0) out vec4 OutColor;

layout (set = 0, binding = 0) uniform sampler2D Sampler2D;

layout (set = 0, binding = 1) uniform LenticularBuffer
{
	float Pitch;
	float Tilt;
	float Center;
	float Subp;

	float DisplayAspect;
	int InvView;
	int Ri;
	int Bi;

	int TileX;
	int TileY;
	float QuiltAspect;
	float Padding;
} LB;

vec2 ToTexcoord(vec3 CoordZ)
{
	return (vec2(mod(CoordZ.z, LB.TileX), -sign(LB.Tilt) * floor(CoordZ.z / LB.TileX)) + CoordZ.xy) / vec2(LB.TileX, LB.TileY);
}

void main()
{
	vec2 UV = InTexcoord;

	//!< 自前で描画しているので、キルトとディスプレイのアスペクト比調整は不要

	if(any(lessThan(UV, vec2(0.0f)))) discard;
	if(any(lessThan(1.0f - UV, vec2(0.0f)))) discard;

	vec3 RGB[3];
	const float XY = LB.TileX * LB.TileY;
	for(int i = 0;i < 3;++i) {
		float Z = (InTexcoord.x + i * LB.Subp + InTexcoord.y * LB.Tilt) * LB.Pitch - LB.Center;
		Z = mod(Z + ceil(abs(Z)), 1.0f) * LB.InvView * XY;

		const float Y = clamp(UV.y, 0.005f, 0.995f);
		const vec3 CoordZ1 = vec3(UV.x, Y, floor(Z));
		const vec3 CoordZ2 = vec3(UV.x, Y, ceil(Z));
		const vec3 Color1 = texture(Sampler2D, ToTexcoord(CoordZ1)).rgb;
		const vec3 Color2 = texture(Sampler2D, ToTexcoord(CoordZ2)).rgb;
		RGB[i] = mix(Color1, Color2, Z - CoordZ1.z);
	}
	OutColor = vec4(RGB[LB.Ri].r, RGB[1].g, RGB[LB.Bi].b, 1.0);
}