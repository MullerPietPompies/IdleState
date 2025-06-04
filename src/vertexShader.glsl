#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix; // NEW

out vec3 FragPos_world;
out vec3 Normal_world;
out vec2 TexCoords;
out vec4 FragPosLightSpace; // NEW: Fragment position in light's clip space

void main() {
    vec4 worldPos_vec4 = model * vec4(aPos, 1.0);
    FragPos_world = worldPos_vec4.xyz;
    Normal_world = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;

    FragPosLightSpace = lightSpaceMatrix * worldPos_vec4; // Calculate this

    gl_Position = projection * view * worldPos_vec4;
}
