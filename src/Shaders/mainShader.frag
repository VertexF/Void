#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColour;

void main()
{
	vec4 tex = texture(texSampler, fragTexCoord);

	if(tex.a == 0)
	{
		discard;
	}

	outColour = vec4(fragColour * tex.rgb, tex.a);
}