#version 460

layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Color;

layout(set=0, binding=0) uniform Uniforms {
    mat4 u_ModelViewMatrix;
    mat4 u_ProjectionMatrix;
};

layout(location=0) out vec3 out_Color;

void main()
{
    gl_Position = u_ProjectionMatrix * u_ModelViewMatrix * vec4(in_Position, 1.0);
    out_Color = in_Color;
}