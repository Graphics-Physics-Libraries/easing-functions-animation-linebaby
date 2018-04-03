#version 330

uniform sampler2D brushTex;
uniform sampler2D maskTex;
uniform float brushAlpha;

in vec2 f_texCoord;

out vec4 displayColor;

void main() {
	displayColor = vec4(
		texture(brushTex, f_texCoord).rrr, 
		min(texture(maskTex, f_texCoord).r, 1-texture(brushTex, f_texCoord).r) - (1 - brushAlpha)
	);
}
