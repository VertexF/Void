#version 450

uint MaterialFeatures_ColourTexture           = 1 << 0;
uint MaterialFeatures_NormalTexture           = 1 << 1;
uint MaterialFeatures_RoughnessTexture        = 1 << 2;
uint MaterialFeatures_OcclusionTexture        = 1 << 3;
uint MaterialFeatures_EmissiveTexture         = 1 << 4;
uint MaterialFeatures_TangentVertexAttribute  = 1 << 5;
uint MaterialFeatures_TexcoordVertexAttribute = 1 << 6; 

layout(std140, binding = 0) uniform LocalConstants
{
    mat4 cameraModel;
    mat4 viewPerspective;
    vec4 eye;
    vec4 light;
};

layout(std140, binding = 1) uniform MaterialConstant
{
    vec4 baseColourFactor;
    mat4 model;
    mat4 modelInv;

    vec3 emissiveFactor;
    float metallicFactor;

    float roughnessFactor;
    float occlusionFactor;
    uint flags;
};

layout(binding = 2) uniform sampler2D diffuseTexture;
layout(binding = 3) uniform sampler2D roughnessMetalnessTexture;
layout(binding = 4) uniform sampler2D occlusionTexture;
layout(binding = 5) uniform sampler2D emissiveTexture;
layout(binding = 6) uniform sampler2D normalTexture;

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
    if ((flags & MaterialFeatures_NormalTexture) != 0) 
    {
        N = normalize(texture(normalTexture, vTexcoord0).rgb * 2.0 - 1.0);
        N = normalize(TBN * N);
    }
    vec3 H = normalize(L + V);

    float roughness = roughnessFactor;
    float metalness = metallicFactor;

    if ((flags & MaterialFeatures_RoughnessTexture) != 0) 
    {
        //Red channel for occlusion value.
        //Green channel contains roughness values.
        //Blue channel contains metalness.
        vec4 rm = texture(roughnessMetalnessTexture, vTexcoord0);

        roughness *= rm.g;
        metalness *= rm.b;
    } 

    float ao = 1.f;
    if ((flags & MaterialFeatures_OcclusionTexture) != 0) 
    {
        ao = texture(occlusionTexture, vTexcoord0).r;
    }

    float alpha = pow(roughness, 2.0);

    vec4 baseColour = baseColourFactor;

    if ((flags & MaterialFeatures_ColourTexture) != 0) 
    {
        vec4 albedo = texture(diffuseTexture, vTexcoord0);
        baseColour *= vec4(decodeSRGB(albedo.rgb), albedo.a);
    }

    vec3 emissive = vec3(0);
    if ((flags & MaterialFeatures_EmissiveTexture) != 0) 
    {
        vec4 e = texture(emissiveTexture, vTexcoord0);

        emissive += decodeSRGB(e.rgb) * emissiveFactor;
    }

    //NOTE: taken from https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specular-brdf
    float NdotH = clamp(dot(N, H), 0, 1);
    float alphaSquared = alpha * alpha;
    float dDenom = (NdotH * NdotH) * (alphaSquared - 1.0) + 1.0;
    float distribution = (alphaSquared * heaviside(NdotH)) / (PI * dDenom * dDenom);

    float lightRange = 10.f;
    float lightIntensity = 10.f;
    vec4 light = {1, 1, 1, 0};

    float NdotL = clamp(dot(N, L), 0, 1);
    float NdotV = clamp(dot(N, V), 0, 1);
    float HdotL = clamp(dot(H, L), 0, 1);
    float HdotV = clamp(dot(H, V), 0, 1);

    float distance = length(light.xyz - vPosition.xyz);
    float intensity = lightIntensity * max(min(1.0 - pow(distance / lightRange, 4.0), 1.0), 0.0) / pow(distance, 2.0);

    if (NdotL > 0.0 || NdotV > 0.0)
    {
        float visibility = (heaviside(HdotL) / (abs(NdotL) + sqrt(alphaSquared + (1.0 - alphaSquared) * (NdotL * NdotL)))) * 
                           (heaviside(HdotV) / (abs(NdotV) + sqrt(alphaSquared + (1.0 - alphaSquared) * (NdotV * NdotV))));

        float specularBrdf = intensity * visibility * distribution;

        vec3 diffuseBrdf = intensity * (1 / PI) * baseColour.rgb;

        //NOTE: f0 in the formula notation refers to the base colour here.
        vec3 conductorFresnel = specularBrdf * (baseColour.rgb + (1.0 - baseColour.rgb) * pow(1.0 - abs(HdotV), 5));

        //NOTE: f0 in the formula notation refers to the value derived from IOR = 1.5.
        float f0 = 0.04;
        float fr = f0 + (1 - f0) * pow(1 - abs(HdotV), 5);
        vec3 fresnelMix = mix(diffuseBrdf, vec3(specularBrdf), fr);

        vec3 materialColour = mix(fresnelMix, conductorFresnel, metalness);

        materialColour = emissive + mix(materialColour, materialColour * ao, occlusionFactor);

        materialColour *= NdotL;

        fragColour = vec4(encodeSRGB(materialColour), baseColour.a);
    }
    else
    {
        fragColour = baseColour;
    }
}