#version 450

#extension GL_EXT_scalar_block_layout : require
// Bindless support
// Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : require

uint MaterialFeatures_TangentVertexAttribute  = 1 << 5;

layout(scalar, set = 0, binding = 0) uniform LocalConstants
{
    mat4 cameraModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(scalar, set = 0, binding = 1) uniform MaterialConstant
{
    mat4 model;
    mat4 modelInv;
    
    uvec4 textures;
    vec4 baseColourFactor;
    vec4 metallicRoughnessOcclusionFactor;
    float alphaCutoff;
    
    vec3 emissiveFactor;
    uint emissiveTextureIndex;
    uint flags;
};

layout(set = 1, binding = 0) uniform sampler2D globalTextures[];
//Alias textures to use the same binding point, as bindless texture is shared
//between all kind of textures: 1d, 2d, 3d.
layout(set = 1, binding = 0) uniform sampler3D globalTextures3D[];

//Write only image do not need formatting in layout.
layout(set = 1, binding = 1) writeonly uniform image2D globalImages2D[];

#define PI 3.1415926538
#define INVALID_TEXTURE_INDEX 65535

layout(location = 0) in vec2 vTexcoord0;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vTangent;
layout(location = 3) in vec4 vPosition;

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

void main()
{
    mat3 TBN = mat3(1.0);
    vec4 baseColour = texture(globalTextures[nonuniformEXT(textures.x)], vTexcoord0) * baseColourFactor;

    //bool useAlphaMask = (flags & DrawFlags_AlphaMask) != 0;
    //if (useAlphaMask && baseColour.a < alphaCutoff)
    if (baseColour.a < alphaCutoff)
    {
        baseColour.a = 0.f;
    }

    if ((flags & MaterialFeatures_TangentVertexAttribute) != 0)
    {
        vec3 tangent = normalize(vTangent.xyz);
        vec3 bitangent = cross(normalize(vNormal), tangent) * vTangent.w;

        TBN = mat3(tangent, bitangent, normalize(vNormal));
    }
    else
    {
        //NOTE: Taken from https://community.khronos.org/t/computing-the-tangent-space-in-the-fragment-shader/52861
        vec3 Q1 = dFdx(vPosition.xyz);
        vec3 Q2 = dFdy(vPosition.xyz);
        vec2 st1 = dFdx(vTexcoord0);
        vec2 st2 = dFdy(vTexcoord0);

        vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
        vec3 B = normalize(-Q1 * st2.s + Q2 * st1.s);

        //The transpose of texture-to-eye space matrix.
        TBN = mat3(T, B, normalize(vNormal));
    }

    vec3 V = normalize(eye.xyz - vPosition.xyz);
    vec3 L = normalize(light.xyz - vPosition.xyz);
    //NOTE: Normal textures are encoded to [0, 1] but we need it to be maped to [-1, 1] value.
    vec3 N = normalize(vNormal);
    if (textures.z != INVALID_TEXTURE_INDEX) 
    {
        N = normalize(texture(globalTextures[nonuniformEXT(textures.z)], vTexcoord0).rgb * 2.0 - 1.0);
        N = normalize(TBN * N);
    }
    vec3 H = normalize(L + V);

    float metalness = metallicRoughnessOcclusionFactor.x;
    float roughness = metallicRoughnessOcclusionFactor.y;

    if (textures.y != INVALID_TEXTURE_INDEX) 
    {
        //Red channel for occlusion value.
        //Green channel contains roughness values.
        //Blue channel contains metalness.
        vec4 rm = texture(globalTextures[nonuniformEXT(textures.y)], vTexcoord0);

        roughness *= rm.g;
        metalness *= rm.b;
    } 

    float occlusion = metallicRoughnessOcclusionFactor.z;
    if (textures.w != INVALID_TEXTURE_INDEX) 
    {
        vec4 o = texture(globalTextures[nonuniformEXT(textures.w)], vTexcoord0);
        occlusion *= o.r;
    }

    float alpha = pow(roughness, 2.0);

    baseColour.rgb = decodeSRGB(baseColour.rgb);

    vec3 emissive = vec3(0);
    if (emissiveTextureIndex != INVALID_TEXTURE_INDEX) 
    {
        vec4 e = texture(globalTextures[nonuniformEXT(emissiveTextureIndex)], vTexcoord0);

        emissive += decodeSRGB(e.rgb) * emissiveFactor;
    }

    //NOTE: taken from https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
    float NdotH = clamp(dot(N, H), 0, 1);
    float alphaSquared = alpha * alpha;
    float dDenom = (NdotH * alphaSquared - NdotH) * NdotH + 1.0;
    float distribution = (alphaSquared) / (PI * dDenom * dDenom);

    float lightRange = 100.f;
    float lightIntensity = 40.f;
    vec4 light = {2, 1, 1, 0};

    float NdotV = abs(dot(N, V)) + 1e-5;
    float NdotL = clamp(dot(N, L), 0, 1);
    float HdotL = clamp(dot(H, L), 0, 1);
    float HdotV = clamp(dot(H, V), 0, 1);

    float distance = length(light.xyz - vPosition.xyz);
    float intensity = lightIntensity * max(min(1.0 - pow(distance / lightRange, 4.0), 1.0), 0.0) / pow(distance, 2.0);

    //Slower but more accurate.
    float GGXL = NdotV * sqrt((-NdotL * alphaSquared + NdotL) * NdotL + alphaSquared);
    float GGXV = NdotL * sqrt((-NdotV * alphaSquared + NdotL) * NdotL + alphaSquared);
    float visibility = 0.5 / (GGXV + GGXL);

    //Faster but less accurate.
    //float GGXL = NdotV * (NdotV * (1.0 - roughnessFactor)) + roughnessFactor;
    //float GGXV = NdotL * (NdotL * (1.0 - roughnessFactor)) + roughnessFactor;
    //float visibility = 0.5 / (GGXV + GGXL);

    float specularBrdf = (visibility * distribution);

    vec3 diffuseBrdf = (1 / PI) * baseColour.rgb;

    //NOTE: f0 in the formula notation refers to the base colour here.
    vec3 conductorFresnel = specularBrdf * (baseColour.rgb + (1.0 - baseColour.rgb) * pow(1.0 - abs(HdotV), 5));

    //NOTE: f0 in the formula notation refers to the value derived from IOR = 1.5.
    float f0 = 0.04;
    float fr = f0 + (1 - f0) * pow(1 - abs(HdotV), 5);
    vec3 fresnelMix = mix(diffuseBrdf, vec3(specularBrdf), fr);

    vec3 materialColour = intensity * mix(fresnelMix, conductorFresnel, metalness) * NdotL;
    
    //materialColour = emissive + mix(materialColour, materialColour * ao, occlusionFactor);
    materialColour = emissive + mix(materialColour, materialColour, occlusion);
        
    fragColour = vec4(encodeSRGB(materialColour), baseColour.a);
}