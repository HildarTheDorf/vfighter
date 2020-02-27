#version 460

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;

layout(set=0, binding=0) uniform Uniforms {
    mat4 u_ModelViewMatrix;
    mat4 u_ProjectionMatrix;
};

layout(location=0) out vec4 out_Normal;

void main()
{
    gl_Position = u_ProjectionMatrix * u_ModelViewMatrix * vec4(in_Position, 1.0);
    out_Normal = transpose(inverse(u_ModelViewMatrix)) * vec4(in_Normal, 1.0);
}