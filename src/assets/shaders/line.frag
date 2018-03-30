#version 330

in vec3 f_color;

out vec4 displayColor;

void main() {
	displayColor = vec4(f_color, 1);
}
