#version 410 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

void main() {
    gl_Position = vec4(1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, 1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    gl_Position = vec4(-1.0, -1.0, 0.5, 1.0);
    EmitVertex();

    EndPrimitive(); 
}
