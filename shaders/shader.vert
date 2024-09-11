#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 camera;
    mat4 rotation;
} ubo;

layout(set = 2, binding = 1) uniform ObjectObject {
    mat4 translation;
    mat4 rotation;
} obo;

layout(set = 3, binding = 2) uniform ModelObject {
    mat4 translation;
    mat4 rotation;
} mbo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 outPosition;
layout(location = 3) out vec3 outNormal;

void main() {
    gl_Position = ubo.proj * ubo.rotation * ubo.camera * mbo.translation * mbo.rotation * obo.translation * obo.rotation * vec4(inPosition, 1.0);

    fragTexCoord = inTexCoord;
    outPosition = (-mbo.rotation * vec4(inPosition, 0.0)).xyz;
    outNormal = (mbo.rotation * vec4(inNormal, 0.0)).xyz;
}