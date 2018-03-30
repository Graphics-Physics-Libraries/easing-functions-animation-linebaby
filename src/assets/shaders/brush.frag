#version 330

uniform sampler2D brushTex;

in vec2 f_texCoord;

out vec4 displayColor;

void main() {
	displayColor = vec4(texture(brushTex, f_texCoord).rrr, 1.0);
}
