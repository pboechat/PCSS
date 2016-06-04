#version 330 core

in vec3 position; 
in vec3 normal;
in vec2 texcoords;

out vec2 vTexcoords; 
out vec3 vNormal;
out vec3 vViewDir;
out vec3 vWorldPosition;
out vec3 vCameraPosition;

uniform mat4 model; 
uniform mat4 view; 
uniform mat4 projection; 
uniform vec3 eyePosition;

void main()
{
    vTexcoords = texcoords;
	vNormal = (model * vec4(normal, 0)).xyz;
	vec4 worldPosition = model * vec4(position, 1.0f);
	vWorldPosition = worldPosition.xyz;
	vec4 cameraPosition = view * model * vec4(position, 1.0f);
	vCameraPosition = cameraPosition.xyz;
	vViewDir = normalize(eyePosition - vWorldPosition);
    gl_Position = projection * cameraPosition;
}
