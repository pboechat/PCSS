#pragma once

#include <GLFW/glfw3.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>

#include "GLUtils.h"
#include "IMovable.h"

class Navigator : public IMovable
{
private:
	float moveSpeed, rotateSpeed;
	int lastPosX, lastPosY;
	unsigned drag;
	float phi;
	float theta;
	glm::vec3 u;
	glm::vec3 v;
	glm::vec3 w;
	glm::vec3 position;
	float moveX;
	float moveZ;

	void rotateH(float delta)
	{
		phi = glm::fmod(phi + delta, 2.0f * glm::pi<float>());
	}

	void rotateV(float delta)
	{
		theta = glm::fmod(theta + delta, 2.0f * glm::pi<float>());
	}

	void pan(float x, float y)
	{
		position += x * u + y * v;
	}

	void walk(float delta)
	{
		position += delta * w;
	}

	void updateAxis()
	{
		float cp = glm::cos(phi);
		float sp = glm::sin(phi);
		float ct = glm::cos(theta);
		float st = glm::sin(theta);

		w = glm::vec3(ct * cp, st, ct * sp);
		v = -glm::vec3(-st * cp, ct, -st * sp);
		u = cross(v, w);
	}

public:
	Navigator(float moveSpeed, float rotateSpeed, const glm::vec3& position = glm::vec3(0, 0, 0), float phi = glm::pi<float>() * 0.5f, float theta = 0) :
		moveSpeed(moveSpeed),
		rotateSpeed(rotateSpeed),
		position(position),
		phi(phi),
		theta(theta),
		drag(0),
		moveX(0),
		moveZ(0)
	{
		updateAxis();
	}

	glm::mat4 getLocalToWorldTransform() const
	{
		return glm::mat4(u.x, v.x, w.x, 0,
			u.y, v.y, w.y, 0,
			u.z, v.z, w.z, 0,
			-glm::dot(u, position), -glm::dot(v, position), -glm::dot(w, position), 1);
	}

	virtual glm::vec3 getPosition() const
	{
		return position;
	}

	virtual void setPosition(const glm::vec3& position)
	{
		this->position = position;
	}

	void buttonDown(int button)
	{
		if (button == GLFW_MOUSE_BUTTON_1)
			drag |= 1;
		else if (button == GLFW_MOUSE_BUTTON_2)
			drag |= 2;
		else if (button == GLFW_MOUSE_BUTTON_3)
			drag |= 4;
	}

	void buttonUp(int button)
	{
		if (button == GLFW_MOUSE_BUTTON_1)
			drag &= ~1;
		else if (button == GLFW_MOUSE_BUTTON_2)
			drag &= ~2;
		else if (button == GLFW_MOUSE_BUTTON_3)
			drag &= ~4;
	}

	void mouseMove(int x, int y)
	{
		if (drag)
		{
			int dX = x - lastPosX, dY = y - lastPosY;
			if (drag & 1) // left
			{
				rotateH(dX * rotateSpeed);
				rotateV(dY * rotateSpeed);
			}
			if (drag & 2) // right
			{
				int absDX = std::abs(dX), absDY = std::abs(dY);
				walk(((absDY > absDX) ? (dY < 0 ? 1.0f : -1.0f) : (dX > 0.0f ? 1.0f : -1.0f)) * std::sqrt(static_cast<float>(dX * dX + dY * dY)) * moveSpeed * 0.007f);
			}
			if (drag & 4) // middle
			{
				pan(dX * moveSpeed * 0.007f, -dY * moveSpeed * 0.007f);
			}
			updateAxis();
		}
		lastPosX = x;
		lastPosY = y;
	}

	void mouseWheel(int delta)
	{
		walk(delta * 0.007f);
		updateAxis();
	}

	void keyDown(int key)
	{
		switch (key)
		{
		case GLFW_KEY_LEFT:
		case GLFW_KEY_A:
			moveX = moveSpeed;
			break;
		case GLFW_KEY_UP:
		case GLFW_KEY_W:
			moveZ = -moveSpeed;
			break;
		case GLFW_KEY_DOWN:
		case GLFW_KEY_S:
			moveZ = moveSpeed;
			break;
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_D:
			moveX = -moveSpeed;
			break;
		}
	}

	void keyUp(int key)
	{
		switch (key)
		{
		case GLFW_KEY_LEFT:
		case GLFW_KEY_A:
			moveX = 0;
			break;
		case GLFW_KEY_UP:
		case GLFW_KEY_W:
			moveZ = 0;
			break;
		case GLFW_KEY_DOWN:
		case GLFW_KEY_S:
			moveZ = 0;
			break;
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_D:
			moveX = 0;
			break;
		}
	}

	void update(float deltaTime)
	{
		if (moveX != 0)
			pan(moveX * moveSpeed * deltaTime, 0.0f);
		if (moveZ != 0)
			walk(moveZ * moveSpeed * deltaTime);
		if (moveX != 0 || moveZ != 0)
			updateAxis();
	}


	inline glm::vec3 forward() const
	{
		return -w;
	}

};

