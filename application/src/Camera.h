#pragma once

#include <cmath>

#include <glm/glm.hpp>

struct Camera
{
	float fovY;
	float zn;
	float zf;

	Camera(float fovY, float zn, float zf)
	{
		this->fovY = fovY;
		this->zn = zn;
		this->zf = zf;
	}

	glm::mat4 getProjection(float aspectRatio)
	{
		float yScale = std::tan(1.0f / (glm::radians(fovY) * 0.5f));
		float xScale = yScale / aspectRatio;
		return glm::mat4(xScale, 0, 0, 0,
			0, yScale, 0, 0,
			0, 0, zf / (zn - zf), -1,
			0, 0, zn * zf / (zn - zf), 0);
	}

};

