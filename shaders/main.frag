#version 460

layout(constant_id = 0) const uint sc_NumLights = 1;
layout(constant_id = 1) const uint sc_MaxMaterials = 1;

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
    Light u_Lights[sc_NumLights];
    Material u_Materials[sc_MaxMaterials];
};

layout(location=0) out vec4 out_Color;

void main()
{
    const Material material = u_Materials[u_MaterialIndex];
    const Light light = u_Lights[0];

    vec3 normal = normalize(in_Normal);
    vec3 lightDir = light.position - in_Position;
    float distance = length(lightDir);
    distance = distance * distance;
    lightDir = normalize(lightDir);

    float lambertian = max(dot(lightDir, normal), 0.0);
    float specular = 0.0;

    if (lambertian > 0.0)
    {
        vec3 viewDir = normalize(-in_Position);

        vec3 halfDir = normalize(lightDir + viewDir);
        float specAngle = max(dot(halfDir, normal), 0.0);
        specular = pow(specAngle, material.shininess);
    }

    vec3 color = material.ambient +
                        material.diffuse * lambertian * light.color * light.power / distance +
                       material.specular * specular * light.color * light.power / distance;
    
    if (material.shininess == 16)
    {
        out_Color = vec4(color, 1.0);
    }
    else
    {
        out_Color = vec4(1.0, 0.0, 0.0, 1.0);     
    }
}