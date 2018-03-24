#version 330

uniform sampler2D tex;
in vec2 fTexCoord;
in vec4 fColor;

out vec4 displayColor;

void main() {
	displayColor = fColor * texture(tex, fTexCoord.st);
}
