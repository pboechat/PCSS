#version 330 core

#define MAX_NUM_LIGHT_SOURCES 8

#define DIRECTIONAL_LIGHT 1
#define POINT_LIGHT 2

#define NEAR 0.1

// NOTE: display modes
#define HARD_SHADOWS 0
#define SOFT_SHADOWS 1
#define BLOCKER_SEARCH 2
#define PENUMBRA_ESTIMATE 3

in vec2 vTexcoords;
in vec3 vNormal;
in vec3 vViewDir;
in vec3 vWorldPosition;
in vec3 vCameraPosition;

struct LightSource
{
	vec3 diffuseColor;
	float diffusePower;
	vec3 specularColor;
	float specularPower;
	vec3 position;
	int type;
	float size;

};

layout (std140) uniform LightSources
{
    LightSource lightSources[MAX_NUM_LIGHT_SOURCES];

};

uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform sampler2D shadowMap4;
uniform sampler2D shadowMap5;
uniform sampler2D shadowMap6;
uniform sampler2D shadowMap7;
uniform samplerCube shadowCubeMap0;
uniform samplerCube shadowCubeMap1;
uniform samplerCube shadowCubeMap2;
uniform samplerCube shadowCubeMap3;
uniform samplerCube shadowCubeMap4;
uniform samplerCube shadowCubeMap5;
uniform samplerCube shadowCubeMap6;
uniform samplerCube shadowCubeMap7;
uniform mat4 shadowMapViewProjection0;
uniform mat4 shadowMapViewProjection1;
uniform mat4 shadowMapViewProjection2;
uniform mat4 shadowMapViewProjection3;
uniform mat4 shadowMapViewProjection4;
uniform mat4 shadowMapViewProjection5;
uniform mat4 shadowMapViewProjection6;
uniform mat4 shadowMapViewProjection7;
uniform float directionalLightShadowMapBias;
uniform float pointLightShadowMapBias;

uniform mat4 invView;
uniform mat4 lightProjection;
uniform vec3 eyePosition;
uniform vec3 ambientColor = vec3(0.1,0.1,0.1);
uniform vec3 specularColor = vec3(1,1,1);
uniform float specularity = 0;
uniform float frustumSize = 1;
uniform sampler2D tex0;
uniform sampler1D distribution0;
uniform sampler1D distribution1;
uniform int numBlockerSearchSamples = 1;
uniform int numPCFSamples = 1;
uniform int displayMode = 0;
uniform int selectedLightSource = -1;

out vec3 outColor;

//////////////////////////////////////////////////////////////////////////
vec2 RandomDirection(sampler1D distribution, float u)
{
   return texture(distribution, u).xy * 2 - vec2(1);
}

/*vec3 DisturbDirection(vec3 direction, sampler1D distribution, float u)
{
	// TODO:
	return direction;
}*/

//////////////////////////////////////////////////////////////////////////
vec3 BlinnPhong(vec3 materialDiffuseColor, 
	vec3 materialSpecularColor, 
	float materialSpecularity, 
	vec3 lightDiffuseColor, 
	float lightDiffusePower, 
	vec3 lightSpecularColor, 
	float lightSpecularPower,
	float NdotL,
	float distanceAttenuation)
{
	return materialDiffuseColor * lightDiffuseColor * lightDiffusePower * NdotL / distanceAttenuation +
					pow(NdotL, materialSpecularity) * materialSpecularColor *
						lightSpecularColor * lightSpecularPower / distanceAttenuation;
}

//////////////////////////////////////////////////////////////////////////
vec3 LightContribution(vec3 diffuseColor, int i)
{
	switch (lightSources[i].type)
	{
	case DIRECTIONAL_LIGHT:
		return BlinnPhong(diffuseColor,
					specularColor,
					specularity,
					lightSources[i].diffuseColor,
					lightSources[i].diffusePower,
					lightSources[i].specularColor,
					lightSources[i].specularPower,
					max(0, dot(vNormal, normalize(vViewDir - lightSources[i].position))),
					1);
	case POINT_LIGHT:
		vec3 lightDir = lightSources[i].position - vWorldPosition;
		float lightDist = length(lightDir);
		lightDir /= lightDist;
		return BlinnPhong(diffuseColor,
					specularColor,
					specularity,
					lightSources[i].diffuseColor,
					lightSources[i].diffusePower,
					lightSources[i].specularColor,
					lightSources[i].specularPower,
					max(0, dot(vNormal, normalize(lightDir + vViewDir))),
					lightDist * lightDist);
	default:
		return vec3(0);
	}
}

//////////////////////////////////////////////////////////////////////////
vec3 ShadowCoords(mat4 shadowMapViewProjection)
{
	vec4 projectedCoords = shadowMapViewProjection * vec4(vWorldPosition, 1);
	vec3 shadowCoords = projectedCoords.xyz / projectedCoords.w;
	shadowCoords = shadowCoords * 0.5 + 0.5;
	return shadowCoords;
}

//////////////////////////////////////////////////////////////////////////
bool IsLightEnabled(int i)
{
	return lightSources[i].type != 0;
}

//////////////////////////////////////////////////////////////////////////
// this search area estimation comes from the following article: 
// http://developer.download.nvidia.com/whitepapers/2008/PCSS_DirectionalLight_Integration.pdf
float SearchWidth(float uvLightSize, float receiverDistance)
{
	return uvLightSize * (receiverDistance - NEAR) / eyePosition.z;
}

//////////////////////////////////////////////////////////////////////////
float Depth(vec3 pos)
{
    vec3 absPos = abs(pos);
	float z = -max(absPos.x, max(absPos.y, absPos.z));
	vec4 clip = lightProjection * vec4(0.0, 0.0, z, 1.0);
	return (clip.z / clip.w) * 0.5 + 0.5;
}

//////////////////////////////////////////////////////////////////////////
float FindBlockerDistance_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
{
	int blockers = 0;
	float avgBlockerDistance = 0;
	float searchWidth = SearchWidth(uvLightSize, shadowCoords.z);
	for (int i = 0; i < numBlockerSearchSamples; i++)
	{
		float z = texture(shadowMap, shadowCoords.xy + RandomDirection(distribution0, i / float(numBlockerSearchSamples)) * searchWidth).r;
		if (z < (shadowCoords.z - directionalLightShadowMapBias))
		{
			blockers++;
			avgBlockerDistance += z;
		}
	}
	if (blockers > 0)
		return avgBlockerDistance / blockers;
	else
		return -1;
}

/*float FindBlockerDistance_PointLight(vec3 direction, float receiverDistance, samplerCube shadowCubeMap, float uvLightSize)
{
	int blockers = 0;
	float avgBlockerDistance = 0;
	float searchWidth = SearchWidth(uvLightSize, receiverDistance);
	for (int i = 0; i < numBlockerSearchSamples; i++)
	{
		float z = texture(shadowCubeMap, DisturbDirection(direction, distribution0, i / float(numBlockerSearchSamples)) * searchWidth).r;
		if (z < (receiverDistance - pointLightShadowMapBias))
		{
			blockers++;
			avgBlockerDistance += z;
		}
	}
	if (blockers > 0)
		return avgBlockerDistance / blockers;
	else
		return -1;
}*/

//////////////////////////////////////////////////////////////////////////
float PCF_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvRadius)
{
	float sum = 0;
	for (int i = 0; i < numPCFSamples; i++)
	{
		float z = texture(shadowMap, shadowCoords.xy + RandomDirection(distribution1, i / float(numPCFSamples)) * uvRadius).r;
		sum += (z < (shadowCoords.z - directionalLightShadowMapBias)) ? 1 : 0;
	}
	return sum / numPCFSamples;
}

/*float PCF_PointLight(vec3 direction, float receiverDistance, samplerCube shadowCubeMap, float uvRadius)
{
	float sum = 0;
	for (int i = 0; i < numPCFSamples; i++)
	{
		float z = texture(shadowCubeMap, DisturbDirection(direction, distribution1, i / float(numPCFSamples)) * uvRadius).r;
		sum += (z < (receiverDistance - pointLightShadowMapBias)) ? 1 : 0;
	}
	return sum / numPCFSamples;
}*/

//////////////////////////////////////////////////////////////////////////
float ShadowMapping_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
{
	float z = texture(shadowMap, shadowCoords.xy).x;
	return (z < (shadowCoords.z - directionalLightShadowMapBias)) ? 0 : 1;
}

float ShadowMapping_PointLight(vec3 lightPosition, samplerCube shadowCubeMap, float uvLightSize)
{
	mat4 lightView = mat4(1,0,0,0, 
		0,1,0,0, 
		0,0,1,0, 
		-lightPosition.x,-lightPosition.y,-lightPosition.z, 1);
	vec3 positionLightSpace = (lightView * invView * vec4(vCameraPosition, 1)).xyz;
	float receiverDistance = Depth(positionLightSpace);
	float z = texture(shadowCubeMap, positionLightSpace).r;
	return (z < (receiverDistance - pointLightShadowMapBias)) ? 0 : 1;
}

//////////////////////////////////////////////////////////////////////////
float PCSS_DirectionalLight(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
{
	// blocker search
	float blockerDistance = FindBlockerDistance_DirectionalLight(shadowCoords, shadowMap, uvLightSize);
	if (blockerDistance == -1)
		return 1;		

	// penumbra estimation
	float penumbraWidth = (shadowCoords.z - blockerDistance) / blockerDistance;

	// percentage-close filtering
	float uvRadius = penumbraWidth * uvLightSize * NEAR / shadowCoords.z;
	return 1 - PCF_DirectionalLight(shadowCoords, shadowMap, uvRadius);
}

float PCSS_PointLight(vec3 lightPosition, samplerCube shadowCubeMap, float uvLightSize)
{
	mat4 lightView = mat4(1,0,0,0, 
		0,1,0,0, 
		0,0,1,0, 
		-lightPosition.x,-lightPosition.y,-lightPosition.z, 1);
	vec3 positionLightSpace = (lightView * invView * vec4(vCameraPosition, 1)).xyz;
	float receiverDistance = Depth(positionLightSpace);
	float z = texture(shadowCubeMap, positionLightSpace).r;
	return (z < (receiverDistance - pointLightShadowMapBias)) ? 0 : 1;
}

//////////////////////////////////////////////////////////////////////////
float HardShadow(int i)
{
	if (i == 0)
	{
		if (IsLightEnabled(0))
		{
			switch (lightSources[0].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection0), shadowMap0, lightSources[0].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[0].position, shadowCubeMap0, lightSources[0].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 1)
	{
		if (IsLightEnabled(1))
		{
			switch (lightSources[1].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection1), shadowMap1, lightSources[1].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[1].position, shadowCubeMap1, lightSources[1].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 2)
	{
		if (IsLightEnabled(2))
		{
			switch (lightSources[2].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection2), shadowMap2, lightSources[2].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[2].position, shadowCubeMap2, lightSources[2].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 3)
	{
		if (IsLightEnabled(3))
		{
			switch (lightSources[3].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection3), shadowMap3, lightSources[3].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[3].position, shadowCubeMap3, lightSources[3].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 4)
	{
		if (IsLightEnabled(4))
		{
			switch (lightSources[4].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection4), shadowMap4, lightSources[4].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[4].position, shadowCubeMap4, lightSources[4].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 5)
	{
		if (IsLightEnabled(5))
		{
			switch (lightSources[5].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection5), shadowMap5, lightSources[5].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[5].position, shadowCubeMap5, lightSources[5].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 6)
	{
		if (IsLightEnabled(6))
		{
			switch (lightSources[6].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection6), shadowMap6, lightSources[6].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[6].position, shadowCubeMap6, lightSources[6].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 7)
	{
		if (IsLightEnabled(7))
		{
			switch (lightSources[7].type)
			{
			case DIRECTIONAL_LIGHT:
				return ShadowMapping_DirectionalLight(ShadowCoords(shadowMapViewProjection7), shadowMap7, lightSources[7].size / frustumSize);
			case POINT_LIGHT:
				return ShadowMapping_PointLight(lightSources[7].position, shadowCubeMap7, lightSources[7].size / frustumSize);
			}
		}
		return 0;
	}
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////
float SoftShadow(int i)
{
	if (i == 0)
	{
		if (IsLightEnabled(0))
		{
			switch (lightSources[0].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection0), shadowMap0, lightSources[0].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[0].position, shadowCubeMap0, lightSources[0].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 1)
	{
		if (IsLightEnabled(1))
		{
			switch (lightSources[1].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection1), shadowMap1, lightSources[1].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[1].position, shadowCubeMap1, lightSources[1].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 2)
	{
		if (IsLightEnabled(2))
		{
			switch (lightSources[2].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection2), shadowMap2, lightSources[2].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[2].position, shadowCubeMap2, lightSources[2].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 3)
	{
		if (IsLightEnabled(3))
		{
			switch (lightSources[3].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection3), shadowMap3, lightSources[3].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[3].position, shadowCubeMap3, lightSources[3].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 4)
	{
		if (IsLightEnabled(4))
		{
			switch (lightSources[4].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection4), shadowMap4, lightSources[4].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[4].position, shadowCubeMap4, lightSources[4].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 5)
	{
		if (IsLightEnabled(5))
		{
			switch (lightSources[5].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection5), shadowMap5, lightSources[5].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[5].position, shadowCubeMap5, lightSources[5].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 6)
	{
		if (IsLightEnabled(6))
		{
			switch (lightSources[6].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection6), shadowMap6, lightSources[6].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[6].position, shadowCubeMap6, lightSources[6].size / frustumSize);
			}
		}
		return 0;
	}
	else if (i == 7)
	{
		if (IsLightEnabled(7))
		{
			switch (lightSources[7].type)
			{
			case DIRECTIONAL_LIGHT:
				return PCSS_DirectionalLight(ShadowCoords(shadowMapViewProjection7), shadowMap7, lightSources[7].size / frustumSize);
			case POINT_LIGHT:
				return PCSS_PointLight(lightSources[7].position, shadowCubeMap7, lightSources[7].size / frustumSize);
			}
		}
		return 0;
	}
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////
void DisplayHardShadows()
{
	vec3 diffuseColor = texture(tex0, vTexcoords).rgb;
	int enabledLights = 0;
	for (int i = 0; i < MAX_NUM_LIGHT_SOURCES; i++)
		if (IsLightEnabled(i))
			enabledLights++;
	if (enabledLights > 0)
	{
		for (int i = 0; i < MAX_NUM_LIGHT_SOURCES; i++)
			outColor += LightContribution(diffuseColor, i) * HardShadow(i);
		outColor /= enabledLights;
	}
	outColor += ambientColor;
}

//////////////////////////////////////////////////////////////////////////
void DisplaySoftShadows()
{
	vec3 diffuseColor = texture(tex0, vTexcoords).rgb;
	int enabledLights = 0;
	for (int i = 0; i < MAX_NUM_LIGHT_SOURCES; i++)
		if (IsLightEnabled(i))
			enabledLights++;
	if (enabledLights > 0)
	{
		for (int i = 0; i < MAX_NUM_LIGHT_SOURCES; i++)
			outColor += LightContribution(diffuseColor, i) * SoftShadow(i);
		outColor /= enabledLights;
	}
	outColor += ambientColor;
}

//////////////////////////////////////////////////////////////////////////
void DisplayBlockerSearch()
{
	float blockerDistance = -1;
	if (selectedLightSource == 0 && IsLightEnabled(0))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection0), shadowMap0, lightSources[0].size / frustumSize);
	else if (selectedLightSource == 1 && IsLightEnabled(1))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection1), shadowMap1, lightSources[1].size / frustumSize);
	else if (selectedLightSource == 2 && IsLightEnabled(2))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection2), shadowMap2, lightSources[2].size / frustumSize);
	else if (selectedLightSource == 3 && IsLightEnabled(3))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection3), shadowMap3, lightSources[3].size / frustumSize);
	else if (selectedLightSource == 4 && IsLightEnabled(4))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection4), shadowMap4, lightSources[4].size / frustumSize);
	else if (selectedLightSource == 5 && IsLightEnabled(5))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection5), shadowMap5, lightSources[5].size / frustumSize);
	else if (selectedLightSource == 6 && IsLightEnabled(6))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection6), shadowMap6, lightSources[6].size / frustumSize);
	else if (selectedLightSource == 7 && IsLightEnabled(7))
		blockerDistance = FindBlockerDistance_DirectionalLight(ShadowCoords(shadowMapViewProjection7), shadowMap7, lightSources[7].size / frustumSize);
	if (blockerDistance == -1)
		outColor = vec3(1);
	else
		outColor = vec3(blockerDistance);
}

//////////////////////////////////////////////////////////////////////////
float PenumbraWidth(vec3 shadowCoords, sampler2D shadowMap, float uvLightSize)
{
	// blocker search
	float blockerDistance = FindBlockerDistance_DirectionalLight(shadowCoords, shadowMap, uvLightSize);
	if (blockerDistance == -1)
		return -1;	
	// penumbra estimation
	return (shadowCoords.z - blockerDistance) / blockerDistance;
}

//////////////////////////////////////////////////////////////////////////
void DisplayPenumbraEstimate()
{
	float penumbraWidth = -1;
	if (selectedLightSource == 0 && IsLightEnabled(0))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection0), shadowMap0, lightSources[0].size / frustumSize);
	else if (selectedLightSource == 1 && IsLightEnabled(1))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection1), shadowMap1, lightSources[1].size / frustumSize);
	else if (selectedLightSource == 2 && IsLightEnabled(2))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection2), shadowMap2, lightSources[2].size / frustumSize);
	else if (selectedLightSource == 3 && IsLightEnabled(3))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection3), shadowMap3, lightSources[3].size / frustumSize);
	else if (selectedLightSource == 4 && IsLightEnabled(4))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection4), shadowMap4, lightSources[4].size / frustumSize);
	else if (selectedLightSource == 5 && IsLightEnabled(5))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection5), shadowMap5, lightSources[5].size / frustumSize);
	else if (selectedLightSource == 6 && IsLightEnabled(6))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection6), shadowMap6, lightSources[6].size / frustumSize);
	else if (selectedLightSource == 7 && IsLightEnabled(7))
		penumbraWidth = PenumbraWidth(ShadowCoords(shadowMapViewProjection7), shadowMap7, lightSources[7].size / frustumSize);
	if (penumbraWidth == -1)
		outColor = vec3(0);
	else
		outColor = vec3(penumbraWidth);
}

//////////////////////////////////////////////////////////////////////////
void main()
{
	switch (displayMode)
	{
	case HARD_SHADOWS:
		DisplayHardShadows();
		break;
	case SOFT_SHADOWS:
		DisplaySoftShadows();
		break;
	case BLOCKER_SEARCH:
		DisplayBlockerSearch();
		break;
	case PENUMBRA_ESTIMATE:
		DisplayPenumbraEstimate();
		break;
	default:
		// FIXME: checking invariant
		outColor = vec3(1,0,0);
	}
}
