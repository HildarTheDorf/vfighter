#version 460

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;

layout(set=0, binding=0) uniform TransformUniforms {
    mat4 u_ModelViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_NormalMatrix;
};

layout(location=0) out vec3 out_Position;
layout(location=1) out vec3 out_Normal;

void main()
{
    vec4 worldPosition = u_ModelViewMatrix * vec4(in_Position, 1.0);
    out_Position = worldPosition.xyz / worldPosition.w;
    gl_Position = u_ProjectionMatrix * worldPosition;

    out_Normal = vec3(u_NormalMatrix * vec4(in_Normal, 1.0));
}