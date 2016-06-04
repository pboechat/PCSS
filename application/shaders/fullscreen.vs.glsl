#version 330 core

const vec2 vertices[4] = vec2[4]( vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0) );

out vec2 vTexcoord;

void main()
{
	vec2 vertex = vertices[gl_VertexID];
	vTexcoord = vertex * 0.5 + 0.5;
    gl_Position = vec4(vertex, 0.0, 1.0);
}