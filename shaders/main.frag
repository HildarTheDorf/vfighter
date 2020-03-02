#version 460

layout(constant_id = 0) const uint MAX_LIGHTS = 2;
const uint MAX_MATERIALS = 1;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 color;
    float power;
};

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;

layout(push_constant) uniform PushConstants {
    uint u_MaterialIndex;
};

layout(std140, set=0, binding=1) uniform LightingUniforms {
    Light u_Lights[MAX_LIGHTS];
    Material u_Materials[MAX_MATERIALS];
};

layout(location=0) out vec4 out_Color;

vec3 calculate_lighting(const in Material material, const in Light light)
{
    vec3 normal = normalize(in_Normal);
    vec3 lightRel = light.position - in_Position;
    vec3 lightDir = normalize(lightRel);
    
    float distance2 = pow(length(lightRel), 2);
    float lambertian = max(dot(lightDir, normal), 0.0);
    float specular = 0.0;

    if (lambertian > 0.0)
    {
        vec3 viewDir = normalize(-in_Position);
        vec3 halfDir = normalize(lightDir + viewDir);
        float specAngle = max(dot(halfDir, normal), 0.0);

        specular = pow(specAngle, material.shininess);
    }

    vec3 baseColor = material.diffuse * lambertian + material.specular * specular;
    return baseColor * light.color * light.power / distance2;
}

void main()
{
    const Material material = u_Materials[u_MaterialIndex];

    vec3 color = material.ambient;
    for (uint i = 0; i < u_Lights.length(); ++i)
    {
        color += calculate_lighting(material, u_Lights[i]);
    }

    out_Color = vec4(color, 1.0);
}