#version 330

uniform mat4 projection;
uniform vec2 translation;
uniform vec2 scale;
uniform float rotation;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;

out vec2 f_texCoord;

mat2 rotate2d(float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
}

void main() {
	f_texCoord = texCoord;
	gl_Position = projection * vec4(((position*scale*rotate2d(rotation)) + translation).xy, 0, 1);
}
