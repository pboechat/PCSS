#version 330 core

in vec2 vTexcoord;

uniform sampler2D shadowMap;

out vec3 outColor;

void main()
{
	outColor = vec3(texture(shadowMap, vTexcoord).r);
}