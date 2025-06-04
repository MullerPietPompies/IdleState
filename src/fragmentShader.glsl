#version 330 core
out vec4 FragColor;

in vec3 FragPos_world; // Make sure this is world space position
in vec3 Normal_world;  // Make sure this is world space normal
in vec2 TexCoords;
in vec4 FragPosLightSpace; // Position of fragment in light's clip space

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
    float shininess;
};
uniform Material material;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

struct PointLight {
    // ... (PointLight struct members) ...
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    bool enabled;
};

uniform vec3 viewPos;
uniform bool isGlass;

#define MAX_DIR_LIGHTS 1
#define MAX_POINT_LIGHTS 20 // Ensure this matches C++
uniform int numDirLights;
uniform DirLight dirLights[MAX_DIR_LIGHTS];
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

// NEW: Shadow map sampler
uniform sampler2D shadowMap;

// Ambient control factors (as before)
const float generalAmbientBaseFactor = 0.001;
const float lightAmbientStrengthMultiplier = 1.0; // Assuming C++ ambient values are very low


// Shadow Calculation Function
float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // Transform to [0,1] range

    if (projCoords.z > 1.0) return 0.0; // Outside far plane of light

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    // Bias to prevent shadow acne
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    // For orthographic projection and directional light, a constant bias can sometimes work:
    // float bias = 0.005;


    float shadow = 0.0;
    if (currentDepth - bias > closestDepth) {
        shadow = 1.0; // In shadow
    }
    return shadow;
}


vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 albedoColor, vec3 specularColorFactor, float shadowContribution) {
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * albedoColor;
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularColorFactor;
    vec3 ambient = light.ambient * albedoColor * lightAmbientStrengthMultiplier;
    return (ambient + (diffuse + specular) * (1.0 - shadowContribution)); // Apply shadow
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedoColor, vec3 specularColorFactor) {
    // Note: This simple shadow map setup is for ONE directional light.
    // Point light shadows are more complex (omnidirectional) and not handled here.
    if (!light.enabled) return vec3(0.0);
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * albedoColor;
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularColorFactor;
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    vec3 ambient = light.ambient * albedoColor * lightAmbientStrengthMultiplier;
    return (ambient + diffuse + specular) * attenuation;
}

void main() {
    vec3 norm = normalize(Normal_world); // Use world space normal
    vec3 viewDir = normalize(viewPos - FragPos_world);

    if (isGlass) {
        // ... (Your glass rendering - typically doesn't receive shadows or casts them differently)
        // For simplicity, glass is not affected by these shadows
        vec4 glassColor = vec4(0.8, 0.9, 1.0, 0.2);
        float fresnel = pow(1.0 - max(0.0, dot(norm, viewDir)), 5.0);
        vec3 dominantLightDir = normalize(vec3(0.0, 1.0, 0.0));
        vec3 reflectDirGlass = reflect(-dominantLightDir, norm);
        float specGlass = pow(max(dot(viewDir, reflectDirGlass), 0.0), 96.0);
        vec3 result = glassColor.rgb * 0.1;
        result += vec3(1.0) * fresnel * 0.6;
        result += vec3(1.0) * specGlass * 0.7;
        // Add point light specular for glass (simplified)
        for(int i = 0; i < min(numPointLights, MAX_POINT_LIGHTS) && i < 2; i++) {
            if(pointLights[i].enabled){
                vec3 pointLightDir = normalize(pointLights[i].position - FragPos_world);
                vec3 pointReflectDir = reflect(-pointLightDir, norm);
                float pointSpec = pow(max(dot(viewDir, pointReflectDir), 0.0), 128.0);
                float distance = length(pointLights[i].position - FragPos_world);
                float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * distance +
                                pointLights[i].quadratic * (distance * distance));
                result += vec3(1.0) * pointSpec * attenuation * 0.5;
            }
        }
        result = pow(result, vec3(1.0/2.2));
        FragColor = vec4(result, glassColor.a);

    } else {
        vec4 albedoSample = texture(material.texture_diffuse1, TexCoords);
        vec3 specularMapColor = texture(material.texture_specular1, TexCoords).rgb;
        float finalAlpha = albedoSample.a;

        if (finalAlpha < 0.05) discard;

        vec3 albedoColor = albedoSample.rgb;
        if (length(albedoColor.rgb) < 0.01) albedoColor = vec3(0.8);

        vec3 specularColorFactor = (length(specularMapColor) < 0.01) ? vec3(1.0) : specularMapColor;

        // Calculate shadow factor from the first directional light
        float shadow = 0.0;
        if (numDirLights > 0 && dirLights[0].enabled) {
            shadow = CalculateShadow(FragPosLightSpace, norm, normalize(-dirLights[0].direction));
        }

        vec3 totalLighting = generalAmbientBaseFactor * albedoColor;

        for (int i = 0; i < min(numDirLights, MAX_DIR_LIGHTS); ++i) {
            // Apply shadow only to the first directional light (the one casting shadows)
            float currentLightShadowFactor = (i == 0) ? shadow : 0.0;
            totalLighting += CalcDirLight(dirLights[i], norm, viewDir, albedoColor, specularColorFactor, currentLightShadowFactor);
        }
        for (int i = 0; i < min(numPointLights, MAX_POINT_LIGHTS); ++i) {
            totalLighting += CalcPointLight(pointLights[i], norm, FragPos_world, viewDir, albedoColor, specularColorFactor);
        }

        // totalLighting = max(totalLighting, vec3(0.01) * albedoColor); // Optional min brightness
        totalLighting = pow(totalLighting, vec3(1.0/2.2));
        FragColor = vec4(totalLighting, finalAlpha);
    }
}
