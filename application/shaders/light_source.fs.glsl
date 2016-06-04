#version 330 core

in vec2 vTexcoords;
in vec3 vNormal;
in vec3 vViewDir;
in vec3 vLightDir;

uniform vec3 color = vec3(1, 1, 1);
uniform float transparency = 0.25f;

out vec4 outColor;

void main()
{
	float a = transparency * dot(vNormal, vViewDir);
	outColor = vec4(color, a);
}
