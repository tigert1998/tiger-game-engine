#version 460 core

in vec3 vPosition;
out vec4 color;

void main() {
	color = vec4(vPosition, 1.0);
}
