#pragma once

#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "GLUtils.h"

struct Mesh
{
	GLuint vao;
	GLuint positionBuffer;
	GLuint texcoordsBuffer;
	GLuint normalBuffer;
	GLuint indexBuffer;
	bool hasIndexBuffer;
	size_t numVertices;
	size_t numIndices;

	Mesh(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec2>& uvs, const std::vector<glm::vec3>& normals) : hasIndexBuffer(false), numIndices(0)
	{
		glGenVertexArrays(1, &vao);

		numVertices = vertices.size();
		glGenBuffers(1, &positionBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
		glBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &texcoordsBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, texcoordsBuffer);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

		glGenBuffers(1, &normalBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

		checkOpenGLError();
	}

	Mesh(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec2>& uvs, const std::vector<glm::vec3>& normals, const std::vector<unsigned>& indices) : Mesh(vertices, uvs, normals)
	{
		hasIndexBuffer = true;

		numIndices = indices.size();
		glGenBuffers(1, &indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(unsigned), &indices[0], GL_STATIC_DRAW);

		checkOpenGLError();
	}

	virtual ~Mesh()
	{
		glDeleteBuffers(1, &positionBuffer);
		glDeleteBuffers(1, &texcoordsBuffer);
		glDeleteBuffers(1, &normalBuffer);

		if (hasIndexBuffer)
			glDeleteBuffers(1, &indexBuffer);

		glDeleteVertexArrays(1, &vao);
	}


	void setup(GLuint program)
	{
		glBindVertexArray(vao);

		// Specify the layout of the vertex data
		GLint positionAttribute = glGetAttribLocation(program, "position");
		if (positionAttribute != -1)
		{
			glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
			glEnableVertexAttribArray(positionAttribute);
			glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
		}

		GLint texcoordsAttribute = glGetAttribLocation(program, "texcoords");
		if (texcoordsAttribute != -1)
		{
			glBindBuffer(GL_ARRAY_BUFFER, texcoordsBuffer);
			glEnableVertexAttribArray(texcoordsAttribute);
			glVertexAttribPointer(texcoordsAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
		}

		GLint normalAttribute = glGetAttribLocation(program, "normal");
		if (normalAttribute != -1)
		{
			glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
			glEnableVertexAttribArray(normalAttribute);
			glVertexAttribPointer(normalAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
		}

		if (hasIndexBuffer)
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

		checkOpenGLError();
	}


	void draw() const
	{
		glBindVertexArray(vao);
		if (hasIndexBuffer)
			glDrawElements(GL_TRIANGLES, (GLsizei)numIndices, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)numVertices);

		checkOpenGLError();
	}

};

