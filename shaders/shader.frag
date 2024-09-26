#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 positionIn;
layout(location = 3) in vec3 normalIn;
layout(location = 4) in vec3 inAmbient;
layout(location = 5) in vec3 inSpecular;
layout(location = 6) in vec3 inSpecularExp;

layout(location = 7) in mat4 inTranslation;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 data;
};

layout(set = 4, binding = 5) uniform LightingUBO {
    Light lights[16];
    int num_lights;
} ubo;

vec3 calculateAmbient(vec4 color, float intensity) {
    vec4 col = color * intensity;
    return col.xyz;
}

vec3 calculatePointLight(
        vec3 normal,
        vec3 position,
        vec3 light_color,
        vec3 diffuse_color,
        float intensity,
        float type
) {
    vec3 light_direction = position - positionIn;
    float distance_to_light = pow(length(light_direction), 2.0);
    light_direction = normalize(light_direction);
    normal = normalize(normal);

    float lambert_value = max(dot(light_direction, normal), 0);
    float specular_value = 0.0;

    if (type == 0.0) { // lambert shading
        return inAmbient * 0.0 + diffuse_color * lambert_value;
    }

    if (lambert_value == 0.0) {
        return inAmbient * 0.0 + diffuse_color * lambert_value * light_color * intensity / distance_to_light +
                             inSpecular * specular_value * light_color * intensity / distance_to_light;
    }

    vec4 pos = inTranslation * vec4(positionIn, 0.0);
    vec3 look_direction = normalize(-(pos.xyz / pos.w));

    if (type == 1.0) { // blinn-phong shading
        vec3 halfway = normalize(light_direction + look_direction);
        float angle = max(dot(halfway, normal), 0.0);
        specular_value = pow(angle, inSpecularExp.x);

        return inAmbient * 0.0 + diffuse_color * lambert_value * light_color * intensity / distance_to_light +
                              inSpecular * specular_value * light_color * intensity / distance_to_light;
    }

    if (type == 2.0) { // phong shading
        vec3 reflection = reflect(-light_direction, normal);
        float angle = max(dot(reflection, look_direction), 0.0);
        specular_value = pow(angle, inSpecularExp.x/4.0);

        return inAmbient * 0.0 + diffuse_color * lambert_value * light_color * intensity / distance_to_light +
                             inSpecular * specular_value * light_color * intensity / distance_to_light;
    }

    return vec3(1, 0, 0);
}

bool point_in_light(
        vec4 dir,
        vec4 pos,
        float fov
) {
    pos = pos - (inTranslation * vec4(positionIn, 0));
    float d = dot(normalize(dir), normalize(-pos));
    d = acos(d);
    d = degrees(d);
    return true;
}

void main() {
    vec4 diffuse_color = texture(texSampler, fragTexCoord);
    outColor = vec4(0, 0, 0, 0);

    if (inSpecularExp.y > 0) { // no light
        outColor = diffuse_color;
        return;
    }

    for (int i = 0; i < ubo.num_lights; ++i) {
        if (ubo.lights[i].data.w == 0) { // ambient light
             outColor += vec4(calculateAmbient(ubo.lights[i].color, ubo.lights[i].data.y), 0);
             outColor /= 2;
        } else if (ubo.lights[i].data.w == 1) { // point light
            outColor += vec4(calculatePointLight(
                normalIn,
                (ubo.lights[i].position).xyz,
                ubo.lights[i].color.xyz,
                diffuse_color.xyz,
                ubo.lights[i].data.y,
                ubo.lights[i].data.z
            ), 0);
            outColor /= 2;
        } else if (ubo.lights[i].data.w == 2) {
            return;
            bool inside_cone = point_in_light(
                ubo.lights[i].direction,
                ubo.lights[i].position,
                ubo.lights[i].data.x
            );

            if (inside_cone) {

                outColor += vec4(calculatePointLight(
                     normalIn,
                     (ubo.lights[i].position).xyz,
                     ubo.lights[i].color.xyz,
                     diffuse_color.xyz,
                     ubo.lights[i].data.y,
                     ubo.lights[i].data.z
                 ), 0);
                outColor /= 2;
            }
        } else {
            outColor = vec4(normalIn, 0);
        }
    }

    outColor = vec4(pow(outColor.xyz, vec3(1.0 / 2.2)), 0);
}