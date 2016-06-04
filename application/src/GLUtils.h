#pragma once

#include <iostream>
#include <GL/glew.h>

//////////////////////////////////////////////////////////////////////////
void checkOpenGLError()
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
		std::cout << "GL_ERROR: " << error << std::endl;
}
