#pragma once

#include <iostream>
#include <fstream>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "GLUtils.h"

struct Shader
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint program;
	GLint uModel;
	GLint uView;
	GLint uProjection;
	GLint uEye;
	GLint uLightPosition;
	GLint uLightIntensity;
	GLint uLightColor;

	Shader(const std::string& vertexShaderFilename, const std::string& fragmentShaderFilename)
	{
		std::fstream fileStream(vertexShaderFilename);
		if (!fileStream.is_open())
		{
			std::cout << "cannot open vertex shader file (" << vertexShaderFilename << ")" << std::endl;
			exit(EXIT_FAILURE);
		}
		std::string fileContent((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
		const char* pVertexSource = fileContent.c_str();
		fileStream.close();

		vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &pVertexSource, NULL);
		glCompileShader(vertexShader);

		checkCompileError(vertexShader, vertexShaderFilename);

		fileStream.open(fragmentShaderFilename);
		if (!fileStream.is_open())
		{
			std::cout << "cannot open fragment shader file (" << fragmentShaderFilename << ")" << std::endl;
			exit(EXIT_FAILURE);
		}
		fileContent = std::string((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
		const char* pFragmentSource = fileContent.c_str();

		fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &pFragmentSource, NULL);
		glCompileShader(fragmentShader);

		checkCompileError(fragmentShader, fragmentShaderFilename);

		program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glBindFragDataLocation(program, 0, "outColor");
		glLinkProgram(program);

		checkLinkError(vertexShaderFilename, fragmentShaderFilename);
	}

	virtual ~Shader()
	{
		glDeleteProgram(program);
		glDeleteShader(fragmentShader);
		glDeleteShader(vertexShader);
	}

	operator GLuint()
	{
		return program;
	}

private:
	void checkLinkError(const std::string& fileName1, const std::string& fileName2)
	{
		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
			std::cout << "error linking " << fileName1 << " and " << fileName2 << std::endl;
			glDeleteProgram(program);
		}
	}

	void checkCompileError(GLuint shader, const std::string& fileName)
	{
		GLint success = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (success != GL_TRUE)
		{
			std::cout << "error compiling " << fileName << std::endl;
			GLint logSize = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
			std::vector<GLchar> errorLog(logSize);
			glGetShaderInfoLog(shader, logSize, &logSize, &errorLog[0]);
			for (int i = 0; i < logSize; i++) std::cout << errorLog[i];
			std::cout << std::endl;
			std::cin.get();
		}
	}

};

