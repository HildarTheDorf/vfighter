#version 460

layout(location=0) in vec4 in_Normal;

layout(location=0) out vec4 out_Color;

void main()
{
    out_Color = in_Normal;
}