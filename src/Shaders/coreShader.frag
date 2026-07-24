#version 460

#extension GL_EXT_scalar_block_layout : require
// Bindless support
// Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

struct SceneData
{
    mat4 view;
    mat4 project;
    mat4 globalModel;
    vec4 eye;
    vec4 light;
};

struct Vertices
{
	float pad;
};

struct ModelPosition
{
	float pad;
};

layout(scalar, buffer_reference) readonly buffer VertexData
{
    Vertices vertexData[];
};

layout(scalar, buffer_reference) readonly buffer ModelPositionData
{
    ModelPosition modelPositions[];
};

layout(scalar, buffer_reference, buffer_reference_align = 8) readonly buffer SceneBufferData
{
    SceneData sceneData;
};

layout(scalar, set = 1, binding = 0) uniform MaterialConstant
{
    mat4 model;
    mat4 modelInv;
    
    uvec4 textures;
    vec4 baseColourFactor;
    vec4 metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    float iorFactor;
    
    vec3 emissiveFactor;
    uint emissiveTextureIndex;
    vec3 specularValue;
    uint flags;
};

layout(set = 0, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 0, binding = 0) uniform sampler3D globalTextures3D[];

layout(set = 0, binding = 0) uniform samplerCube globalTexturesCube[];

//Write only image do not need formatting in layout.
layout(set = 0, binding = 1) writeonly uniform image2D globalImages2D[];

#define PI 3.1415926538
#define INVALID_TEXTURE_INDEX 65535

layout(location = 0) in vec2 vTexcoord0;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vTangent;
layout(location = 3) in vec4 vPosition;
layout(location = 4) in vec4 vColour;

layout(location = 0) out vec4 fragColour;

#define PI 3.1415926538

vec3 decodeSRGB(vec3 colour)
{
    vec3 result;
    if (colour.r <= 0.04045)
    {
        result.r = colour.r / 12.92;
    }
    else
    {
        result.r = pow((colour.r + 0.055) / 1.055, 2.4);
    }

    if (colour.g <= 0.04045)
    {
        result.g = colour.g / 12.92;
    }
    else
    {
        result.g = pow((colour.g + 0.055) / 1.055, 2.4);
    }

    if (colour.b <= 0.04045)
    {
        result.b = colour.b / 12.92;
    }
    else
    {
        result.b = pow((colour.b + 0.055) / 1.055, 2.4);
    }

    return clamp(result, 0.0, 1.0);
}

vec3 encodeSRGB(vec3 colour)
{
    vec3 result;
    if (colour.r <= 0.0031308)
    {
        result.r = colour.r * 12.92;
    }
    else
    {
        result.r = 1.055 * pow(colour.r, 1.0 / 2.4) - 0.055;
    }

    if (colour.g <= 0.0031308)
    {
        result.g = colour.g * 12.92;
    }
    else
    {
        result.g = 1.055 * pow(colour.g, 1.0 / 2.4) - 0.055;
    }

    if (colour.b <= 0.0031308)
    {
        result.b = colour.b * 12.92;
    }
    else
    {
        result.b = 1.055 * pow(colour.b, 1.0 / 2.4) - 0.055;
    }

    return clamp(result, 0.0, 1.0);
}

float heaviside(float value)
{
    if (value > 0.0)
    {
        return 1.0;
    }
    return 0.0;
}

layout(scalar, push_constant) uniform entityIndex
{
    VertexData vertexDataReference;
    ModelPositionData modelPositionsReference;
    SceneBufferData sceneBufferReference;
};

float distributionGGX(float NoH, float roughness)
{
	float a = NoH * roughness;
	float k = roughness / (1.0 - NoH * NoH + a * a);
	return k * k * (1.0 / PI);
}

float visibilitySmithGGXCorrelated(float NoV, float NoL, float roughness)
{
	float alpha2 = roughness * roughness;
	float GGXV = NoL * sqrt(NoV * NoV * (1 - alpha2) + alpha2);
	float GGXL = NoV * sqrt(NoL * NoL * (1 - alpha2) + alpha2);
	return 0.5 / (GGXV + GGXL);
}

vec3 fresnelSchlick(float v, vec3 f0)
{
	float f = pow(1.0 - v, 5.0);
	return f + f0 * (1.0 - f);
}

float diffuseLambert()
{
    return 1.0 / PI;
}

float directionalLight(vec3 lightDirection, vec3 normal, float lightIntensity)
{
    vec3 l = normalize(-lightDirection);
    float NoL = clamp(dot(normal, l), 0.0, 1.0);

    return lightIntensity * NoL;
}

float getSquareFalloffAttenuation(vec3 posToLight, float lightInvRadius)
{
	float distanceSquare = dot(posToLight, posToLight);
	float factor = distanceSquare * lightInvRadius * lightInvRadius;
	float smoothFactor = max(1.0 - factor * factor, 0.0);
	return (smoothFactor * smoothFactor) / max(distanceSquare, 1e-4);
}

float getSpotAngleAttenuation(vec3 l, vec3 lightDir, float innerAngle, float outerAngle)
{
	//The scale and offset computations can be done CPU-side.
	float cosOuter = cos(outerAngle);
	float spotScale = 1.0 / max(cos(innerAngle) - cosOuter, 1e-4);
	float spotOffset = -cosOuter * spotScale;
	
	float cd = dot(normalize(-lightDir), l);
	float attenuation = clamp(cd * spotScale + spotOffset, 0.0, 1.0);
	return attenuation * attenuation;
}

float spotLight()
{
    float lightIntensity = 10000.f;
    float lightInvRadius = 1 / 500.f;
    vec3 lightDir = -sceneBufferReference.sceneData.eye.xyz;
    float innerAngle = PI / 20;
    float outerAngle = PI / 6;
    vec3 posToLight = sceneBufferReference.sceneData.light.xyz - vPosition.xyz;
    vec3 L = normalize(posToLight);
	
	float attenuation = getSquareFalloffAttenuation(posToLight, lightInvRadius) * getSpotAngleAttenuation(L, lightDir, innerAngle, outerAngle);
    return lightIntensity * attenuation;
}

void main()
{
//Only here to turn off lighting if I need it.
//    if(textures.x != INVALID_TEXTURE_INDEX)
//    {
//        fragColour = texture(globalTextures[nonuniformEXT(textures.x)], vTexcoord0) * baseColourFactor;
//        fragColour *= vColour;
//    }
//    else
//    {
//        fragColour = vec4(0.5, 0.5, 0.5, 1.0);
//    }

    mat3 TBN = mat3(1.0);
    vec4 baseColour = vec4(0.5);
    baseColour.a = 1.f;
    baseColour = texture(globalTextures[nonuniformEXT(textures.x)], vTexcoord0) * baseColourFactor;

    //bool useAlphaMask = (flags & DrawFlags_AlphaMask) != 0;
    //if (useAlphaMask && baseColour.a < alphaCutoff)
    if (baseColour.a < alphaCutoff)
    {
        baseColour.a = 0.f;
    }

    vec3 tangent = normalize(vTangent.xyz);
    vec3 bitangent = cross(normalize(vNormal), tangent) * vTangent.w;
    vec3 N = normalize(vNormal);

    TBN = mat3(tangent, bitangent, N);

    vec3 V = normalize(sceneBufferReference.sceneData.eye.xyz - vPosition.xyz);
    vec3 L = normalize(sceneBufferReference.sceneData.light.xyz - vPosition.xyz);
    //NOTE: Normal textures are encoded to [0, 1] but we need it to be maped to [-1, 1] value.
    if (textures.z != INVALID_TEXTURE_INDEX) 
    {
        N = normalize(texture(globalTextures[nonuniformEXT(textures.z)], vTexcoord0).rgb * 2.0 - 1.0);
        N = normalize(TBN * N);
    }

    float metalness = metallicRoughnessOcclusionFactor.x;
    float roughness = metallicRoughnessOcclusionFactor.y;

    //Red channel for occlusion value.
    //Green channel contains roughness values.
    //Blue channel contains metalness.
    vec4 rm = texture(globalTextures[nonuniformEXT(textures.y)], vTexcoord0);

    roughness *= rm.g;
    metalness *= rm.b;

    float occlusion = metallicRoughnessOcclusionFactor.z;
    vec4 o = texture(globalTextures[nonuniformEXT(textures.w)], vTexcoord0);
    occlusion *= o.r;

    float alpha = pow(roughness, 2.0);

    baseColour.rgb = decodeSRGB(baseColour.rgb);

    vec3 emissive = vec3(0);
    vec4 e = texture(globalTextures[nonuniformEXT(emissiveTextureIndex)], vTexcoord0);

    emissive += decodeSRGB(e.rgb) * emissiveFactor;
	
	vec3 H = normalize(L + V);
    float NoV = abs(dot(N, V)) + 1e-5;
    float NoL = clamp(dot(N, L), 0, 1.0);
    float NoH = clamp(dot(N, H), 0, 1.0);
    float LoH = clamp(dot(L, H), 0, 1.0);

    //diffuse BRDF
    vec3 Fd = baseColour.rgb * diffuseLambert();

    vec3 reflecence = (specularValue * specularValue) * 0.16;

    //specular BRDF
    vec3 f0 = mix(vec3(0.04), baseColour.rgb, metalness);
    float D = distributionGGX(NoH, roughness); 
    float G = visibilitySmithGGXCorrelated(NoL, NoV, roughness);
    vec3 F = fresnelSchlick(clamp(dot(H, V), 0, 1.0), f0);

    vec3 specular = ((D * G) * F);
    
    //Lights
	float spotLightIntensity = spotLight();
    float directionalLightIntensity = directionalLight(vec3(-1.0, -2.0, 3.0), N, 10.f);

    vec3 luminance = ((specular + Fd) * spotLightIntensity * NoL);
    vec3 luminance1 = ((specular + Fd) * directionalLightIntensity * NoL) * vec3(1.0, 0.867, 0.684);

    vec3 lightOutput = luminance + luminance1;

    vec3 ambient = occlusion * (baseColour.rgb * 0.01);

    fragColour = vec4(encodeSRGB(lightOutput + ambient + emissive), baseColour.a);
}