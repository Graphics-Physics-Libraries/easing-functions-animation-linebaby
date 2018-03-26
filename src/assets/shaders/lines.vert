#version 330

uniform mat4 projection;

layout (location = 0) in vec2 position;

void main() {
	gl_PointSize = 10.0;
	gl_Position = projection * vec4(position.xy, 0, 1);
}
