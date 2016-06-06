#pragma once

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/common.hpp>

#include "IMovable.h"

struct Animation
{
	Animation(float duration /* seconds */, bool loop, IMovable& target) : duration(duration), loop(loop), target(target), elapsedTime(0), finished(false) {}

	inline bool isFinished()
	{
		return finished;
	}

	virtual void update(float spf)
	{
		if (finished)
			return;
		elapsedTime += spf;
		if (elapsedTime >= duration)
		{
			float newElapsedTime = glm::fmod(elapsedTime, duration);
			if (!loop)
			{
				finished = true;
				spf = glm::max(0.0f, spf - (elapsedTime - newElapsedTime));
			}
			elapsedTime = newElapsedTime;
		}
		animate(spf);
	}

	inline IMovable& getTarget() const
	{
		return target;
	}

protected:
	float duration;
	bool loop;
	IMovable& target;
	float elapsedTime;
	bool finished;

	virtual void animate(float spf) = 0;

};

struct Rotate : public Animation
{
	Rotate(const glm::vec3& axis, float duration /* seconds */, bool loop, IMovable& target) : Animation(duration, loop, target), axis(axis) {}

protected:
	virtual void animate(float spf)
	{
		static glm::mat4 identity(1);
		auto position4 = glm::vec4(target.getPosition(), 1);
		target.setPosition((glm::rotate(identity,
			(spf * glm::two_pi<float>()) * duration,
			axis) * position4).xyz());
	}

private:
	glm::vec3 axis;

};

struct ForthAndBack : public Animation
{
	ForthAndBack(const glm::vec3& direction, float distance /* world units */, float duration /* seconds */, bool loop, IMovable& target) : Animation(duration, loop, target), direction(glm::normalize(direction)), halfDuration(duration * 0.5f), speed(distance / halfDuration) {}

	virtual void animate(float spf)
	{
		auto position = target.getPosition();
		if (elapsedTime < halfDuration)
			target.setPosition(position + (direction * speed * spf));
		else
			target.setPosition(position - (direction * speed * spf));
	}

private:
	glm::vec3 direction; // world units / sec
	float halfDuration;
	float speed;

};

struct ForthStopAndBack : public Animation
{
	ForthStopAndBack(const glm::vec3& direction, float distance /* world units */, float stopDuration, float duration /* seconds */, bool loop, IMovable& target) : Animation(duration, loop, target), direction(glm::normalize(direction)), stopDuration(glm::min(stopDuration, duration)), halfTotalDurationMinusStop(glm::max(0.0f, duration - stopDuration) * 0.5f), speed((halfTotalDurationMinusStop > 0) ? distance / halfTotalDurationMinusStop : 0) {}

	virtual void animate(float spf)
	{
		auto position = target.getPosition();
		if (elapsedTime < halfTotalDurationMinusStop)
			target.setPosition(position + (direction * speed * spf));
		else if (elapsedTime >= halfTotalDurationMinusStop + stopDuration)
			target.setPosition(position - (direction * speed * spf));
	}

private:
	glm::vec3 direction; // world units / sec
	float stopDuration;
	float halfTotalDurationMinusStop;
	float speed;

};
