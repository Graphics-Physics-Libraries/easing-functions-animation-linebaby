#version 330

uniform mat4 projection;

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 color;

out vec2 fTexCoord;
out vec4 fColor;

void main() {
	fTexCoord = texCoord;
	fColor = color;
	gl_Position = projection * vec4(position.xy, 0, 1);
}
