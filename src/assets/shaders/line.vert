#version 330

uniform mat4 projection;
uniform vec3 color;
uniform float pointSize;

out vec3 f_color;

layout (location = 0) in vec2 position;

void main() {
	gl_PointSize = pointSize;
	f_color = color;
	gl_Position = projection * vec4(position.xy, 0, 1);
}
