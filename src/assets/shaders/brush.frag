#version 330

uniform sampler2D brushTex;
uniform sampler2D maskTex;
uniform float brushAlpha = 1.0;
uniform vec4 brushColor = vec4( 0.0, 0.0, 0.0, 1.0 );

in vec2 f_texCoord;

out vec4 displayColor;

void main() {
	float intensity = 1 - texture(brushTex, f_texCoord).r;
	vec4 brush = brushColor * intensity;
	displayColor = vec4(
		brush.rgb,
		min(texture(maskTex, f_texCoord).r, 1-texture(brushTex, f_texCoord).r) - (1 - brushAlpha)
	);
}
