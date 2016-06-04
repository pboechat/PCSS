#version 330 core

out float outDepth;

void main()
{
    outDepth = gl_FragCoord.z;
}