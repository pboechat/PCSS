#define GLEW_STATIC

#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <sstream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <SOIL.h>
#include <AntTweakBar.h>

#include "objloader.hpp"
#include "GLUtils.h"
#include "Mesh.h"
#include "Shader.h"
#include "Navigator.h"
#include "Camera.h"
#include "PoissonGenerator.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define SHADOW_MAP_SIZE 1024
#define DEFAULT_DIRECTIONAL_LIGHT_SHADOW_MAP_BIAS 0.025f
#define DEFAULT_POINT_LIGHT_SHADOW_MAP_BIAS 0.0075f
#define FOV 60.0f
#define NEAR 0.1f
#define FAR 100.0f
#define DEFAULT_OBJ_TEX0_FILENAME "wood.png"
#define DEFAULT_GROUND_TEX0_FILENAME "brick_floor.jpg"
// NOTE: requires change in fragments shaders
#define MAX_NUM_LIGHT_SOURCES 8
#define DEFAULT_NUM_SAMPLES 16
#define MIN_NUM_SAMPLES 4
#define MAX_NUM_SAMPLES 256
#define DEFAULT_LIGHT_SIZE 0.5f

const std::string SHADERS_DIR("shaders/");
const std::string MEDIA_DIR("media/");

//////////////////////////////////////////////////////////////////////////
enum LightType
{
	DIRECTIONAL = 1,
	POINT

};

enum DisplayMode
{
	HARD_SHADOWS = 0,
	SOFT_SHADOWS,
	BLOCKER_SEARCH,
	PENUMBRA_ESTIMATE

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
		diffuseColor(0,0,0),
		diffusePower(0),
		specularColor(0,0,0),
		specularPower(0),
		position(0,0,0),
		type((LightType)0),
		size(0)
	{
	}

	LightSource(LightType type, const glm::vec3& position, float diffusePower) : diffuseColor(glm::vec3(1, 1, 1)),
		diffusePower(diffusePower),
		specularColor(glm::vec3(1, 1, 1)),
		specularPower(0),
		position(position),
		type(type),
		size(DEFAULT_LIGHT_SIZE)
	{
	}

};
#pragma pack(pop)

struct LightSourceAdapter
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

	void animate(float spf)
	{
		static glm::mat4 identity(1);
		static glm::vec3 yAxis(0, 1, 0);
		switch (source.type)
		{
		case DIRECTIONAL:
			source.position = (glm::rotate(identity, 
				(spf * 2.0f * glm::pi<float>()) * 0.25f, // 4 spins per second
				yAxis) * glm::vec4(source.position, 1)).xyz();
			break;
		case POINT:
			elapsedTime = glm::fmod(elapsedTime + spf, 4.0f);
			source.position = (glm::translate(identity, glm::vec3(0, (elapsedTime * 0.5f - 1) * 0.05f, 0))
				* glm::vec4(source.position, 1)).xyz();
			break;
		default:
			// FIXME: checking invariants
			throw std::runtime_error("unknown light type");
		}
	}

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

struct ShadowMap
{
	size_t index;
	bool hasTexture;
	GLuint texture;
	GLint textureLocation;
	glm::mat4 viewProjection;
	GLint viewProjectionLocation;
	bool hasCubeMap;
	GLuint cubeMap;
	GLint cubeMapLocation;

};

//////////////////////////////////////////////////////////////////////////
Camera g_camera(FOV, NEAR, FAR);
Navigator g_navigator(10.0f, 0.01f, glm::vec3(0, 0, 3));
TwType g_vec2Type;
TwType g_vec3Type;
TwType g_vec4Type;
TwType g_lightType;
TwType g_displayModeType;
LightType g_selectedLightType = DIRECTIONAL;
size_t g_selectedLightSource = 0;
std::vector<std::unique_ptr<LightSourceAdapter>> g_lightSources;
std::vector<ShadowMap> g_shadowMaps;
std::vector<TwBar*> g_bars;
char g_tex0Filename[2][256];
bool g_hasTex0[2] = { false, false };
GLuint g_tex0[2] = { 0, 0 };
glm::vec3 g_ambientColor = glm::vec3(0.1f, 0.1f, 0.1f);
glm::vec3 g_specularColor = glm::vec3(1, 1, 1);
float g_specularity = 1;
GLuint g_framebuffer = 0;
float g_directionalLightShadowMapBias = DEFAULT_DIRECTIONAL_LIGHT_SHADOW_MAP_BIAS;
float g_pointLightShadowMapBias = DEFAULT_POINT_LIGHT_SHADOW_MAP_BIAS;
bool g_drawShadowMap = false;
size_t g_shadowMapIndex = 0;
size_t g_numBlockerSearchSamples = DEFAULT_NUM_SAMPLES;
size_t g_numPCFSamples = DEFAULT_NUM_SAMPLES;
int g_screenWidth = SCREEN_WIDTH, g_screenHeight = SCREEN_HEIGHT;
float g_aspectRatio = SCREEN_WIDTH / (float)SCREEN_HEIGHT;
float g_frustumSize = 1;
GLuint g_distributions[2] = { 0, 0 };
DisplayMode g_displayMode = DisplayMode::HARD_SHADOWS;
bool g_animateLights = false;

//////////////////////////////////////////////////////////////////////////
void errorCallback(int error, const char* description)
{
	std::cout << "[GLFW ERROR]: " << description << std::endl;
}

//////////////////////////////////////////////////////////////////////////
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (TwEventKeyGLFW(key, action))
		return;

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GL_TRUE);
		return;
	}

	if (mods == GLFW_MOD_ALT)
	{
		switch (key)
		{
		case GLFW_KEY_LEFT:
		case GLFW_KEY_A:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(-0.07f, 0, 0));
			break;
		case GLFW_KEY_UP:
		case GLFW_KEY_W:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(0, 0, 0.07f));
			break;
		case GLFW_KEY_DOWN:
		case GLFW_KEY_S:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(0, 0, -0.07f));
			break;
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_D:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(0.07f, 0, 0));
			break;
		case GLFW_KEY_KP_ADD:
		case GLFW_KEY_Q:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(0, 0.07f, 0));
			break;
		case GLFW_KEY_KP_SUBTRACT:
		case GLFW_KEY_E:
			if (g_selectedLightSource < g_lightSources.size())
				g_lightSources[g_selectedLightSource]->translate(glm::vec3(0, -0.07f, 0));
			break;
		}
	}
	else
	{
		if (action == GLFW_PRESS)
		{
			g_navigator.keyDown(key);

			switch (key)
			{
			case GLFW_KEY_KP_ADD:
				if (g_selectedLightSource < g_lightSources.size())
					g_lightSources[g_selectedLightSource]->increasePower();
				break;
			case GLFW_KEY_KP_SUBTRACT:
				if (g_selectedLightSource < g_lightSources.size())
					g_lightSources[g_selectedLightSource]->decreasePower();
				break;
			case GLFW_KEY_PAGE_UP:
				g_selectedLightSource = (g_lightSources.empty()) ? 0 : (g_selectedLightSource + 1) % g_lightSources.size();
				break;
			case GLFW_KEY_PAGE_DOWN:
				g_selectedLightSource = (g_selectedLightSource == 0) ? (size_t)std::max(0, (int)g_lightSources.size() - 1) : g_selectedLightSource - 1;
				break;
			}
		}
		else if (action == GLFW_RELEASE)
			g_navigator.keyUp(key);
	}
}

//////////////////////////////////////////////////////////////////////////
void charCallback(GLFWwindow* window, int codePoint)
{
	TwEventCharGLFW(codePoint, GLFW_PRESS);
}

//////////////////////////////////////////////////////////////////////////
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (TwEventMouseButtonGLFW(button, action))
		return;

	if (action == GLFW_PRESS)
		g_navigator.buttonDown(button);
	else if (action == GLFW_RELEASE)
		g_navigator.buttonUp(button);
}

//////////////////////////////////////////////////////////////////////////
void mouseMoveCallback(GLFWwindow* window, double x, double y)
{
	g_navigator.mouseMove((int)x, (int)y);
	TwMouseMotion(int(x), int(y));
}

//////////////////////////////////////////////////////////////////////////
void windowSizeCallback(GLFWwindow* window, int width, int height)
{
	g_screenWidth = width;
	g_screenHeight = height;
	g_aspectRatio = width / (float)height;
	g_frustumSize = 2 * NEAR * std::tanf(FOV * 0.5f) * g_aspectRatio;
	TwWindowSize(g_screenWidth, g_screenHeight);
}

//////////////////////////////////////////////////////////////////////////
void createXZPlane(float width, float depth, int xDivisions, int zDivisions, float uvRepeat, std::vector<glm::vec3>& vertices, std::vector<glm::vec2>& uvs, std::vector<glm::vec3>& normals, std::vector<unsigned>& indices)
{
	float halfWidth = width * 0.5f,
		halfDepth = depth * 0.5f;
	float xStep = width / (float)xDivisions,
		zStep = depth / (float)zDivisions;
	float z = -halfDepth;
	for (int zI = 0; zI <= zDivisions; zI++, z += zStep)
	{
		float x = -halfWidth;
		for (int xI = 0; xI <= xDivisions; xI++, x += xStep)
		{
			vertices.emplace_back(x, 0, z);
			normals.emplace_back(0, 1, 0);
			uvs.emplace_back(x / width * uvRepeat, z / depth * uvRepeat);
		}
	}
	int zW = (xDivisions + 1);
	for (int zI = 0; zI < zDivisions; zI++)
	{
		int zI0 = zI * zW,
			zI1 = zI0 + zW;
		for (int xI = 0; xI < xDivisions; xI++)
		{
			int xzI0 = zI0 + xI,
				xzI1 = zI1 + xI;
			indices.emplace_back(xzI0 + 1);
			indices.emplace_back(xzI0);
			indices.emplace_back(xzI1);
			indices.emplace_back(xzI0 + 1);
			indices.emplace_back(xzI1);
			indices.emplace_back(xzI1 + 1);
		}
	}
}

void defineAnttweakbarStructs()
{
	TwStructMember structMembers1[] = {
		{ "x", TW_TYPE_FLOAT, offsetof(glm::vec2, x), "step=0.01" },
		{ "y", TW_TYPE_FLOAT, offsetof(glm::vec2, y), "step=0.01" }
	};
	g_vec2Type = TwDefineStruct(
		"Vec2Type",
		structMembers1,
		2,
		sizeof(glm::vec2),
		NULL,
		NULL);
	TwStructMember structMembers2[] = {
		{ "x", TW_TYPE_FLOAT, offsetof(glm::vec3, x), "step=0.01" },
		{ "y", TW_TYPE_FLOAT, offsetof(glm::vec3, y), "step=0.01" },
		{ "z", TW_TYPE_FLOAT, offsetof(glm::vec3, z), "step=0.01" }
	};
	g_vec3Type = TwDefineStruct(
		"Vec3Type",
		structMembers2,
		3,
		sizeof(glm::vec3),
		NULL,
		NULL);
	TwStructMember structMembers3[] = {
		{ "x", TW_TYPE_FLOAT, offsetof(glm::vec4, x), "step=0.01" },
		{ "y", TW_TYPE_FLOAT, offsetof(glm::vec4, y), "step=0.01" },
		{ "z", TW_TYPE_FLOAT, offsetof(glm::vec4, z), "step=0.01" },
		{ "w", TW_TYPE_FLOAT, offsetof(glm::vec4, w), "step=0.01" }
	};
	g_vec4Type = TwDefineStruct(
		"Vec4Type",
		structMembers3,
		4,
		sizeof(glm::vec4),
		NULL,
		NULL);
	TwEnumVal enumVals1[] = { { DIRECTIONAL, "Directional" },{ POINT, "Point" } };
	g_lightType = TwDefineEnum("LightType", enumVals1, 2);
	TwEnumVal enumVals2[] = { { DisplayMode::HARD_SHADOWS, "Hard Shadows" }, { DisplayMode::SOFT_SHADOWS, "Soft Shadows" },{ DisplayMode::BLOCKER_SEARCH, "Blocker Search" }, { DisplayMode::PENUMBRA_ESTIMATE, "Penumbra Estimate" } };
	g_displayModeType = TwDefineEnum("DisplayMode", enumVals2, 4);
}

void TW_CALL removeLightCallback(void *clientData)
{
	auto i = *reinterpret_cast<size_t*>(clientData);
	auto it1 = std::find_if(g_lightSources.begin(), g_lightSources.end(), [i](const auto& light) { return light->getIndex() == i; });
	// FIXME: checking invariants
	if (it1 == g_lightSources.end())
		throw std::runtime_error("invalid light index");
	*it1 = nullptr;
	g_lightSources.erase(it1);
	auto it2 = std::find_if(g_shadowMaps.begin(), g_shadowMaps.end(), [i](const auto& shadowMap) { return shadowMap.index == i; });
	// FIXME: checking invariants
	if (it2 == g_shadowMaps.end())
		throw std::runtime_error("invalid shadow map index");
	if (it2->hasTexture)
		glDeleteTextures(1, &it2->texture);
	if (it2->hasCubeMap)
		glDeleteTextures(1, &it2->cubeMap);
	g_shadowMaps.erase(it2);
	g_selectedLightSource = 0;
}

void TW_CALL addLightCallback(void *clientData)
{
	auto i = g_lightSources.size();
	std::string barName = "Light" + std::to_string(i);
	auto newBar = TwNewBar(barName.c_str());
	std::string label = "Light " + std::to_string(i) + "[";
	switch (g_selectedLightType)
	{
	case DIRECTIONAL:
		label += "Directional]";
		break;
	case POINT:
		label += "Point]";
		break;
	default:
		// FIXME: checking invariants
		throw std::runtime_error("unknown light type");
	}
	TwDefine((barName + " label='" + label + "' ").c_str());
	g_lightSources.emplace_back(new LightSourceAdapter(g_selectedLightType, i, newBar));
	g_lightSources[i]->initializeTwBar();
	GLuint shadowMapTexture;
	glGenTextures(1, &shadowMapTexture);
	switch (g_selectedLightType)
	{
	case DIRECTIONAL:
		glBindTexture(GL_TEXTURE_2D, shadowMapTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		g_shadowMaps.emplace_back(ShadowMap{ i, true, shadowMapTexture, -2, glm::mat4(1), -2, false, 0, -2 });
		glBindTexture(GL_TEXTURE_2D, 0);
		break;
	case POINT:
		glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMapTexture);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT32, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		g_shadowMaps.emplace_back(ShadowMap{ i, false, 0, -2, glm::mat4(1), -2, true, shadowMapTexture, -2 });
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		break;
	default:
		// FIXME: checking invariants
		throw std::runtime_error("unknown light type");
	}
	checkOpenGLError();
}

//////////////////////////////////////////////////////////////////////////
template <size_t UniqueIndex>
void TwAddStringVarRO(TwBar* bar, const char* name, const std::string& value, const std::string& def = "")
{
	static char cString[1024];
	auto count = std::min((size_t)1023, value.size());
	strncpy_s(cString, value.c_str(), count);
	cString[count + 1] = (char)'\0';
	TwAddVarRO(bar, name, TW_TYPE_CSSTRING(1024), cString, def.c_str());
}

template <size_t I>
void TW_CALL setTex0Callback(const void* value, void* clientData = 0)
{
	auto ptr = static_cast<const char*>(value);
	strncpy_s(g_tex0Filename[I], ptr, 255);
	g_tex0Filename[I][255] = (char)'/0';
	int width, height;
	auto image = SOIL_load_image((MEDIA_DIR + g_tex0Filename[I]).c_str(), &width, &height, 0, SOIL_LOAD_RGB);
	if (image == nullptr)
		return;
	if (g_hasTex0[I])
		glDeleteTextures(1, &g_tex0[I]);
	g_hasTex0[I] = true;
	glGenTextures(1, &g_tex0[I]);
	glBindTexture(GL_TEXTURE_2D, g_tex0[I]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	SOIL_free_image_data(image);
}

template <size_t I>
void TW_CALL getTex0Callback(void* value, void* clientData)
{
	auto ptr = static_cast<char*>(value);
	strncpy(ptr, g_tex0Filename[I], 256);
}

void createPoissonDiscDistribution(GLuint texture, size_t numSamples)
{
	auto points = PoissonGenerator::GeneratePoissonPoints(numSamples * 2, PoissonGenerator::DefaultPRNG());
	size_t attempts = 0;
	while (points.size() < numSamples && ++attempts < 100)
		points = PoissonGenerator::GeneratePoissonPoints(numSamples * 2, PoissonGenerator::DefaultPRNG());
	if (attempts == 100)
	{
		std::cout << "couldn't generate Poisson-disc distribution with " << numSamples << " samples" << std::endl;
		numSamples = points.size();
	}
	std::vector<float> data(numSamples * 2);
	for (auto i = 0, j = 0; i < numSamples; i++, j += 2)
	{
		auto& point = points[i];
		data[j] = point.x;
		data[j + 1] = point.y;
	}
	glBindTexture(GL_TEXTURE_1D, texture);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RG, numSamples, 0, GL_RG, GL_FLOAT, &data[0]);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void TW_CALL setNumBlockerSearchSamplesCallback(const void* value, void* clientData)
{
	g_numBlockerSearchSamples = *static_cast<const size_t*>(value);
	g_numBlockerSearchSamples = glm::clamp<size_t>(g_numBlockerSearchSamples, MIN_NUM_SAMPLES, MAX_NUM_SAMPLES);
	createPoissonDiscDistribution(g_distributions[0], g_numBlockerSearchSamples);
}

void TW_CALL getNumBlockerSearchSamplesCallback(void* value, void* clientData)
{
	*static_cast<size_t*>(value) = g_numBlockerSearchSamples;
}

void TW_CALL setNumPCFSamplesCallback(const void* value, void* clientData)
{
	g_numPCFSamples = *static_cast<const size_t*>(value);
	g_numPCFSamples = glm::clamp<size_t>(g_numPCFSamples, MIN_NUM_SAMPLES, MAX_NUM_SAMPLES);
	createPoissonDiscDistribution(g_distributions[1], g_numPCFSamples);
}

void TW_CALL getNumPCFSamplesCallback(void* value, void* clientData)
{
	*static_cast<size_t*>(value) = g_numPCFSamples;
}

//////////////////////////////////////////////////////////////////////////
void LightSourceAdapter::initializeTwBar()
{
	TwAddVarRW(bar, "Enabled", TW_TYPE_BOOLCPP, &enabled, "");
	switch (source.type)
	{
	case DIRECTIONAL:
		TwAddVarRO(bar, "Type", TW_TYPE_CSSTRING(1024), "Directional", "");
		TwAddVarRW(bar, "Direction", g_vec3Type, &source.position, "");
		break;
	case POINT:
		TwAddVarRO(bar, "Type", TW_TYPE_CSSTRING(1024), "Point", "");
		TwAddVarRW(bar, "Position", g_vec3Type, &source.position, "");
		break;
	default:
		// TODO:
		throw std::runtime_error("");
	}
	TwAddVarRW(bar, "Diffuse Color", g_vec3Type, &source.diffuseColor, "");
	TwAddVarRW(bar, "Diffuse Power", TW_TYPE_FLOAT, &source.diffusePower, "min=0 step=0.1");
	TwAddVarRW(bar, "Specular Color", g_vec3Type, &source.specularColor, "");
	TwAddVarRW(bar, "Specular Power", TW_TYPE_FLOAT, &source.specularPower, "min=0 step=0.1");
	TwAddVarRW(bar, "Size", TW_TYPE_FLOAT, &source.size, "min=0.1 step=0.1");
	TwAddButton(bar, "Remove", removeLightCallback, (void*)&index, 0);
}

//////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "usage: <obj file> [<directional light shadow map bias>] [<point light shadow map bias>]" << std::endl;
		exit(EXIT_FAILURE);
	}

	if (argc >= 3)
		g_directionalLightShadowMapBias = atof(argv[2]);

	if (argc >= 4)
		g_pointLightShadowMapBias = atof(argv[3]);

	//////////////////////////////////////////////////////////////////////////
	// Initialize GLFW and create window

	if (!glfwInit())
	{
		exit(EXIT_FAILURE);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PCSS", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetErrorCallback(errorCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCharCallback(window, (GLFWcharfun)charCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, mouseMoveCallback);
	glfwSetWindowSizeCallback(window, windowSizeCallback);

	//////////////////////////////////////////////////////////////////////////
	// Initialize GLEW

	glewExperimental = GL_TRUE;
	glewInit();

	//////////////////////////////////////////////////////////////////////////
	// Initialize AntTweakBar
	TwInit(TW_OPENGL_CORE, 0);

	defineAnttweakbarStructs();

	auto bar0 = TwNewBar("Global");
	TwDefine(" Global position='10 10' size='400 400' ");
	TwWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);

	TwAddSeparator(bar0, 0, " group='Scene' ");
	TwAddStringVarRO<0>(bar0, "OBJ", argv[1], "group=Scene");
	setTex0Callback<0>(DEFAULT_OBJ_TEX0_FILENAME);
	setTex0Callback<1>(DEFAULT_GROUND_TEX0_FILENAME);
	TwAddVarCB(bar0, "Diffuse Map (OBJ)", TW_TYPE_CSSTRING(256), setTex0Callback<0>, getTex0Callback<0>, 0, "group=Scene");
	TwAddVarCB(bar0, "Diffuse Map (ground)", TW_TYPE_CSSTRING(256), setTex0Callback<1>, getTex0Callback<1>, 0, "group=Scene");
	TwAddVarRW(bar0, "Ambient Color", g_vec3Type, &g_ambientColor, "group=Scene");
	TwAddVarRW(bar0, "Specular Color", g_vec3Type, &g_specularColor, "group=Scene");
	TwAddVarRW(bar0, "Specularity", TW_TYPE_FLOAT, &g_specularity, "group=Scene");

	TwAddSeparator(bar0, 0, " group='Shadows' ");
	TwAddVarRW(bar0, "Shadow Map Bias (Directional Light)", TW_TYPE_FLOAT, &g_directionalLightShadowMapBias, "step=0.0001 group=Shadows");
	TwAddVarRW(bar0, "Shadow Map Bias (Point Light)", TW_TYPE_FLOAT, &g_pointLightShadowMapBias, "step=0.0001 group=Shadows");
	TwAddVarRW(bar0, "Draw Shadow Map", TW_TYPE_BOOLCPP, &g_drawShadowMap, "group=Shadows");
	TwAddVarRW(bar0, "Shadow Map Index", TW_TYPE_INT32, &g_shadowMapIndex, "group=Shadows");
	std::string definitionStr = "min=" + std::to_string(MIN_NUM_SAMPLES) + " max=" + std::to_string(MAX_NUM_SAMPLES) + " group=Shadows";
	TwAddVarCB(bar0, "# Blocker Search Samples", TW_TYPE_INT32, setNumBlockerSearchSamplesCallback, getNumBlockerSearchSamplesCallback, 0, definitionStr.c_str());
	TwAddVarCB(bar0, "# PCF Samples", TW_TYPE_INT32, setNumPCFSamplesCallback, getNumPCFSamplesCallback, 0, definitionStr.c_str());
	TwAddVarRW(bar0, "Display Mode", g_displayModeType, &g_displayMode, " group=Shadows");

	TwAddSeparator(bar0, 0, " group='Lights' ");
	TwAddVarRW(bar0, "Animate Lights", TW_TYPE_BOOLCPP, &g_animateLights, "group=Lights");
	TwAddVarRW(bar0, "Selected Light", TW_TYPE_INT32, &g_selectedLightSource, "group=Lights");
	TwAddVarRW(bar0, "Light Type", g_lightType, &g_selectedLightType, "group=Lights");
	TwAddButton(bar0, "Add Light", addLightCallback, 0, "group=Lights");

	//////////////////////////////////////////////////////////////////////////
	// Setup basic GL states

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP);

	// NOTE: opening scope so that objects created inside of it can be destroyed before the program ends
	{
		// Reading shader source from files
		Shader shader0(SHADERS_DIR + "shadow_pass.vs.glsl", SHADERS_DIR + "shadow_pass.fs.glsl");
		Shader shader1(SHADERS_DIR + "fullscreen.vs.glsl", SHADERS_DIR + "draw_shadow_map.fs.glsl");
		Shader shader2(SHADERS_DIR + "common.vs.glsl", SHADERS_DIR + "blinn_phong_textured_and_shadowed.fs.glsl");

		//////////////////////////////////////////////////////////////////////////
		// Load OBJ file

		std::vector<glm::vec3> vertices;
		std::vector<glm::vec2> uvs;
		std::vector<glm::vec3> normals;
		if (!loadOBJData((MEDIA_DIR + argv[1]).c_str(), vertices, uvs, normals))
		{
			std::cout << "error loading OBJ" << std::endl;
			exit(EXIT_FAILURE);
		}
		Mesh objMesh(vertices, uvs, normals);

		vertices.clear();
		uvs.clear();
		normals.clear();

		//////////////////////////////////////////////////////////////////////////
		// Create sphere mesh

		std::vector<unsigned> indices;
		createXZPlane(20, 20, 1, 1, 4, vertices, uvs, normals, indices);
		Mesh planeMesh(vertices, uvs, normals, indices);

		// Setting VAO pointers to the allocated VBOs, which is only necessary ONCE since we're always rendering a mesh with the same shader
		objMesh.setup(shader2);
		planeMesh.setup(shader2);

		glGenFramebuffers(1, &g_framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GLint uModelViewProjection0 = glGetUniformLocation(shader0, "modelViewProjection");

		GLint uModel_shader2 = glGetUniformLocation(shader2, "model");
		GLint uView_shader2 = glGetUniformLocation(shader2, "view");
		GLint uInvView_shader2 = glGetUniformLocation(shader2, "invView");
		GLint uProjection_shader2 = glGetUniformLocation(shader2, "projection");
		GLint uLightProjection_shader2 = glGetUniformLocation(shader2, "lightProjection");
		GLint uEyePosition_shader2 = glGetUniformLocation(shader2, "eyePosition");
		GLint uTex0_shader2 = glGetUniformLocation(shader2, "tex0");
		GLint uAmbientColor_shader2 = glGetUniformLocation(shader2, "ambientColor");
		GLint uSpecularColor_shader2 = glGetUniformLocation(shader2, "specularColor");
		GLint uSpecularity_shader2 = glGetUniformLocation(shader2, "specularity");
		GLint uDirectionalLightShadowMapBias_shader2 = glGetUniformLocation(shader2, "directionalLightShadowMapBias");
		GLint uPointLightShadowMapBias_shader2 = glGetUniformLocation(shader2, "pointLightShadowMapBias");
		GLint uFrustumSize_shader2 = glGetUniformLocation(shader2, "frustumSize");
		GLint uDistribution0_shader2 = glGetUniformLocation(shader2, "distribution0");
		GLint uDistribution1_shader2 = glGetUniformLocation(shader2, "distribution1");
		GLint uNumBlockerSearchSamples_shader2 = glGetUniformLocation(shader2, "numBlockerSearchSamples");
		GLint uNumPCFSamples_shader2 = glGetUniformLocation(shader2, "numPCFSamples");
		GLint uDisplayMode_shader2 = glGetUniformLocation(shader2, "displayMode");
		GLint uSelectedLightSource_shader2 = glGetUniformLocation(shader2, "selectedLightSource");

		glm::mat4 objModel(1);
		glm::mat4 planeModel(glm::translate(glm::mat4(1), glm::vec3(0, -0.25f, 0)));

		//////////////////////////////////////////////////////////////////////////
		// Create light sources uniform buffer

		GLuint lightSourcesBlockIndex = glGetUniformBlockIndex(shader2, "LightSources");
		glUniformBlockBinding(shader2, lightSourcesBlockIndex, 0);
		GLuint lightSourcesUniformBuffer = 0;
		if (lightSourcesBlockIndex != GL_INVALID_INDEX)
		{
			glGenBuffers(1, &lightSourcesUniformBuffer);
			glBindBufferBase(GL_UNIFORM_BUFFER, lightSourcesBlockIndex, lightSourcesUniformBuffer);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, lightSourcesUniformBuffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(LightSource) * MAX_NUM_LIGHT_SOURCES, 0, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		//////////////////////////////////////////////////////////////////////////
		// Create Poisson-disc distributions
		glGenTextures(2, g_distributions);
		createPoissonDiscDistribution(g_distributions[0], g_numBlockerSearchSamples);
		createPoissonDiscDistribution(g_distributions[1], g_numPCFSamples);

		while (!glfwWindowShouldClose(window))
		{
			auto start = std::chrono::system_clock::now();

			//////////////////////////////////////////////////////////////////////////
			// Shadow passes

			//glCullFace(GL_FRONT);

			glBindFramebuffer(GL_FRAMEBUFFER, g_framebuffer);
			glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
			for (auto i = 0; i < g_lightSources.size(); i++)
			{
				auto& lightSource = g_lightSources[i];
				if (!lightSource->isEnabled())
					continue;
				auto& shadowMap = g_shadowMaps[i];
				switch (lightSource->getType())
				{
				case DIRECTIONAL:
				{
					glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMap.texture, 0);
					if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
						continue;
					// TODO: compute view projection only when needed
					auto viewProjection = shadowMap.viewProjection = lightSource->getViewProjection();
					glClear(GL_DEPTH_BUFFER_BIT);
					glUseProgram(shader0);
					glUniformMatrix4fv(uModelViewProjection0, 1, GL_FALSE, glm::value_ptr(viewProjection));
					objMesh.draw();
				}
				break;
				case POINT:
				{
					for (int j = 0; j < 6; j++)
					{
						auto textureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + j;
						glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureTarget, shadowMap.cubeMap, 0);
						if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
							continue;
						auto viewProjection = lightSource->getViewProjection(textureTarget);
						glClear(GL_DEPTH_BUFFER_BIT);
						glUseProgram(shader0);
						glUniformMatrix4fv(uModelViewProjection0, 1, GL_FALSE, glm::value_ptr(viewProjection));
						objMesh.draw();
					}
				}
				break;
				// FIXME: checking invariants
				default:
					throw std::runtime_error("unknown light type");
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Forward pass

			//glCullFace(GL_BACK);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, g_screenWidth, g_screenHeight);

			glClearColor(g_ambientColor.r, g_ambientColor.g, g_ambientColor.b, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			if (g_drawShadowMap)
			{
				if (g_shadowMapIndex >= 0 && g_shadowMapIndex < g_lightSources.size())
				{
					glUseProgram(shader1);
					auto uShadowMap = glGetUniformLocation(shader1, "shadowMap");
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, g_shadowMaps[g_shadowMapIndex].texture);
					glUniform1i(uShadowMap, 0);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
				checkOpenGLError();
			}
			else
			{
				auto eyePosition = g_navigator.getPosition();
				auto view = g_navigator.getLocalToWorldTransform();
				auto invView = glm::inverse(view);
				auto projection = g_camera.getProjection(g_aspectRatio);
				auto lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 10.0f);

				//////////////////////////////////////////////////////////////////////////
				// Draw OBJ

				glUseProgram(shader2);

				if (uEyePosition_shader2 != -1)
					glUniform3fv(uEyePosition_shader2, 1, glm::value_ptr(eyePosition));
				if (uView_shader2 != -1)
					glUniformMatrix4fv(uView_shader2, 1, GL_FALSE, glm::value_ptr(view));
				if (uInvView_shader2 != -1)
					glUniformMatrix4fv(uInvView_shader2, 1, GL_FALSE, glm::value_ptr(invView));
				if (uProjection_shader2 != -1)
					glUniformMatrix4fv(uProjection_shader2, 1, GL_FALSE, glm::value_ptr(projection));
				if (uLightProjection_shader2 != -1)
					glUniformMatrix4fv(uLightProjection_shader2, 1, GL_FALSE, glm::value_ptr(lightProjection));
				if (uModel_shader2 != -1)
					glUniformMatrix4fv(uModel_shader2, 1, GL_FALSE, glm::value_ptr(objModel));
				if (uTex0_shader2 != -1)
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, g_tex0[0]);
					glUniform1i(uTex0_shader2, 0);
				}
				if (uAmbientColor_shader2 != -1)
					glUniform3fv(uAmbientColor_shader2, 1, glm::value_ptr(g_ambientColor));
				if (uSpecularColor_shader2 != -1)
					glUniform3fv(uSpecularColor_shader2, 1, glm::value_ptr(g_specularColor));
				if (uSpecularity_shader2 != -1)
					glUniform1f(uSpecularity_shader2, g_specularity);
				if (lightSourcesBlockIndex != GL_INVALID_INDEX)
				{
					glBindBuffer(GL_UNIFORM_BUFFER, lightSourcesUniformBuffer);
					for (auto& lightSource : g_lightSources)
						lightSource->updateData();
				}
				if (uDirectionalLightShadowMapBias_shader2 != -1)
					glUniform1f(uDirectionalLightShadowMapBias_shader2, g_directionalLightShadowMapBias);
				if (uPointLightShadowMapBias_shader2 != -1)
					glUniform1f(uPointLightShadowMapBias_shader2, g_pointLightShadowMapBias);
				if (uFrustumSize_shader2 != -1)
					glUniform1f(uFrustumSize_shader2, g_frustumSize);
				for (auto i = 0; i < g_shadowMaps.size(); i++)
				{
					auto& lightSource = g_lightSources[i];
					if (!lightSource->isEnabled())
						continue;
					auto& shadowMap = g_shadowMaps[i];
					switch (lightSource->getType())
					{
					case DIRECTIONAL:
					{
						auto uShadowMap = shadowMap.textureLocation;
						if (uShadowMap == -2)
							uShadowMap = shadowMap.textureLocation = glGetUniformLocation(shader2, ("shadowMap" + std::to_string(i)).c_str());
						if (uShadowMap == -1)
							continue;
						glActiveTexture(GL_TEXTURE0 + i + 1);
						glBindTexture(GL_TEXTURE_2D, shadowMap.texture);
						glUniform1i(uShadowMap, (GLint)i + 1);
						auto uShadowMapViewProjection = shadowMap.viewProjectionLocation;
						if (uShadowMapViewProjection == -2)
							uShadowMapViewProjection = shadowMap.viewProjectionLocation = glGetUniformLocation(shader2, ("shadowMapViewProjection" + std::to_string(i)).c_str());
						if (uShadowMapViewProjection == -1)
							continue;
						glUniformMatrix4fv(uShadowMapViewProjection, 1, GL_FALSE, glm::value_ptr(shadowMap.viewProjection));
					}
					break;
					case POINT:
					{
						auto uShadowCubeMap = shadowMap.cubeMapLocation;
						if (uShadowCubeMap == -2)
							uShadowCubeMap = shadowMap.cubeMapLocation = glGetUniformLocation(shader2, ("shadowCubeMap" + std::to_string(i)).c_str());
						if (uShadowCubeMap == -1)
							continue;
						glActiveTexture(GL_TEXTURE0 + i + 1);
						glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.cubeMap);
						glUniform1i(uShadowCubeMap, (GLint)i + 1);
					}
					break;
					default:
						// FIXME: checking invariants
						throw std::runtime_error("unknown light type");
					}
				}
				if (uDistribution0_shader2 != -1)
				{
					auto texUnit = g_shadowMaps.size() + 2;
					glActiveTexture(GL_TEXTURE0 + texUnit);
					glBindTexture(GL_TEXTURE_1D, g_distributions[0]);
					glUniform1i(uDistribution0_shader2, texUnit);
				}
				if (uDistribution1_shader2 != -1)
				{
					auto texUnit = g_shadowMaps.size() + 3;
					glActiveTexture(GL_TEXTURE0 + texUnit);
					glBindTexture(GL_TEXTURE_1D, g_distributions[1]);
					glUniform1i(uDistribution1_shader2, texUnit);
				}
				if (uNumBlockerSearchSamples_shader2 != -1)
					glUniform1i(uNumBlockerSearchSamples_shader2, (GLint)g_numBlockerSearchSamples);
				if (uNumPCFSamples_shader2 != -1)
					glUniform1i(uNumPCFSamples_shader2, (GLint)g_numPCFSamples);
				if (uDisplayMode_shader2 != -1)
					glUniform1i(uDisplayMode_shader2, (GLint)g_displayMode);
				if (uSelectedLightSource_shader2 != -1)
					glUniform1i(uSelectedLightSource_shader2, (GLint)g_selectedLightSource);

				objMesh.draw();

				if (uModel_shader2 != -1)
					glUniformMatrix4fv(uModel_shader2, 1, GL_FALSE, glm::value_ptr(planeModel));
				if (uTex0_shader2 != -1)
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, g_tex0[1]);
					glUniform1i(uTex0_shader2, 0);
				}
				if (uSpecularColor_shader2 != -1)
					glUniform3fv(uSpecularColor_shader2, 1, glm::value_ptr(glm::vec3(0, 0, 0)));
				if (uSpecularity_shader2 != -1)
					glUniform1f(uSpecularity_shader2, 0);

				planeMesh.draw();

				checkOpenGLError();
			}

			TwDraw();

			glfwSwapBuffers(window);

			checkOpenGLError();

			auto spf = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() / 1000.0f;

			if (g_animateLights)
				for (auto& lightSource : g_lightSources)
				{
					if (!lightSource->isEnabled())
						continue;
					lightSource->animate(spf);
				}

			g_navigator.update(spf);

			std::string title = "PCSS @ " + std::to_string(1.0f / spf) + " fps";
			glfwSetWindowTitle(window, title.c_str());

			glfwPollEvents();
		}

		if (lightSourcesBlockIndex != GL_INVALID_INDEX)
			glDeleteBuffers(1, &lightSourcesUniformBuffer);

		for (auto& shadowMap : g_shadowMaps)
		{
			if (shadowMap.hasTexture)
				glDeleteTextures(1, &shadowMap.texture);
			if (shadowMap.hasCubeMap)
				glDeleteTextures(1, &shadowMap.cubeMap);
		}

		glDeleteTextures(2, g_distributions);
		glDeleteTextures(1, &g_tex0[0]);
	}

	TwTerminate();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}