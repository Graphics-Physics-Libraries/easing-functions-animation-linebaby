#version 330

in vec2 f_texCoord;

out vec4 displayColor;

uniform sampler2D maskTex;
uniform sampler2D brushTex;

void main() {
	displayColor = vec4(texture(brushTex, f_texCoord).rrr, texture(maskTex, f_texCoord).r);
}
