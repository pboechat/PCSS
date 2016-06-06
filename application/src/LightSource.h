#pragma once

#include <GL/glew.h>
#define GLM_SWIZZLE
#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/common.hpp>
#include <AntTweakBar.h>

#include "IMovable.h"

enum LightType
{
	DIRECTIONAL = 1,
	POINT

};

#pragma pack(push, 16)
struct __declspec(align(16)) LightSource
{
	glm::vec3 diffuseColor;
	float diffusePower;
	glm::vec3 specularColor;
	float specularPower;
	glm::vec3 position;
	LightType type;
	float size;

	LightSource() :
		diffuseColor(0, 0, 0),
		diffusePower(0),
		specularColor(0, 0, 0),
		specularPower(0),
		position(0, 0, 0),
		type((LightType)0),
		size(0)
	{
	}

	LightSource(LightType type, const glm::vec3& position, float diffusePower) : diffuseColor(glm::vec3(1, 1, 1)),
		diffusePower(diffusePower),
		specularColor(glm::vec3(1, 1, 1)),
		specularPower(1),
		position(position),
		type(type),
		size(1)
	{
	}

};
#pragma pack(pop)

struct LightSourceAdapter : public IMovable
{
	LightSourceAdapter(LightType type, size_t index, TwBar* bar) :
		enabled(true),
		index(index),
		bar(bar),
		source(type, (type == DIRECTIONAL) ? glm::vec3(0, -1, 0) : glm::vec3(0, 3, 0), (type == DIRECTIONAL) ? 1 : 10)
	{
	}

	virtual ~LightSourceAdapter()
	{
		LightSource empty;
		glBufferSubData(GL_UNIFORM_BUFFER, index * sizeof(LightSource), sizeof(LightSource), &empty);
		TwDeleteBar(bar);
	}

	inline size_t getIndex() const { return index; }

	inline void translate(const glm::vec3& value)
	{
		source.position += value;
	}

	virtual glm::vec3 getPosition() const
	{
		return source.position;
	}

	virtual void setPosition(const glm::vec3& position)
	{
		source.position = position;
	}

	inline void increasePower()
	{
		source.diffusePower = glm::clamp(source.diffusePower + 0.1f, 0.0f, FLT_MAX);
	}

	inline void decreasePower()
	{
		source.diffusePower = glm::clamp(source.diffusePower - 0.1f, 0.0f, FLT_MAX);
	}

	void initializeTwBar();

	inline void updateData() const
	{
		static LightSource empty;
		if (enabled)
			glBufferSubData(GL_UNIFORM_BUFFER, index * sizeof(LightSource), sizeof(LightSource), &source);
		else
			glBufferSubData(GL_UNIFORM_BUFFER, index * sizeof(LightSource), sizeof(LightSource), &empty);
	}

	inline glm::mat4 getViewProjection(GLuint textureTarget = 0) const
	{
		glm::mat4 projection;
		glm::mat4 view;
		switch (source.type)
		{
		case DIRECTIONAL:
			projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -20.0f, 20.0f);
			view = glm::lookAt(glm::normalize(-source.position), glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0));
			break;
		case POINT:
			projection = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 10.0f);
			switch (textureTarget)
			{
			case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
				break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
				break;
			case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
				break;
			case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
				view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
				break;
			default:
				// FIXME: checking invariants
				throw std::runtime_error("unknown texture target");
			}
			view = glm::translate(view, -source.position);
			break;
		default:
			// FIXME: checking invariants
			throw std::runtime_error("unknown light type");
		}
		return projection * view;
	}

	//void animate(float spf)
	//{
	//	static glm::mat4 identity(1);
	//	static glm::vec3 yAxis(0, 1, 0);
	//	switch (source.type)
	//	{
	//	case DIRECTIONAL:
	//		source.position = (glm::rotate(identity,
	//			(spf * 2.0f * glm::pi<float>()) * 0.25f, // 4 spins per second
	//			yAxis) * glm::vec4(source.position, 1)).xyz();
	//		break;
	//	case POINT:
	//		elapsedTime = glm::fmod(elapsedTime + spf, 4.0f);
	//		source.position = (glm::translate(identity, glm::vec3(0, (elapsedTime * 0.5f - 1) * 0.05f, 0))
	//			* glm::vec4(source.position, 1)).xyz();
	//		break;
	//	default:
	//		// FIXME: checking invariants
	//		throw std::runtime_error("unknown light type");
	//	}
	//}

	bool isEnabled() const
	{
		return enabled;
	}

	LightType getType() const
	{
		return source.type;
	}

protected:
	bool enabled;
	size_t index;
	TwBar* bar;
	LightSource source;
	glm::mat4 model;
	float elapsedTime;

};
