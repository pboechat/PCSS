#pragma once

#include <glm/glm.hpp>

struct IMovable
{
	virtual glm::vec3 getPosition() const = 0;
	virtual void setPosition(const glm::vec3& position) = 0;

};
