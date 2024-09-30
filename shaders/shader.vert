#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 camera;
    mat4 rotation;
    vec4 data;
} ubo;

struct Material {
    vec4 ambient_color;
    vec4 specular_color;
    vec4 specular_exponent;
    vec4 other_data;
};

layout(set = 2, binding = 1) uniform ObjectObject {
    mat4 translation;
    mat4 rotation;

    Material mat;
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
layout(location = 4) out vec3 outAmbient;
layout(location = 5) out vec3 outSpecular;
layout(location = 6) out vec3 outSpecularExp;

layout(location = 7) out mat4 outTranslation;

void main() {
    vec4 final_pos = mbo.translation * mbo.rotation * obo.translation * obo.rotation * vec4(inPosition, 1.0);
    gl_Position =  ubo.proj * ubo.rotation * ubo.camera * final_pos;
    float dist = sqrt(((ubo.data.x) * gl_Position.x * gl_Position.x) + ((ubo.data.x) * gl_Position.y * gl_Position.y) + (gl_Position.z));
    if (ubo.data.y > 0) gl_Position.xy /= dist;

    fragTexCoord = inTexCoord;
    outPosition = final_pos.xyz;
    outNormal = (mbo.rotation * vec4(inNormal, 0.0)).xyz;

    outTranslation = ubo.rotation * ubo.camera;

    outAmbient = obo.mat.ambient_color.xyz;
    outSpecular = obo.mat.specular_color.xyz;
    outSpecularExp = obo.mat.specular_exponent.xyz;
}