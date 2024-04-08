#version 150

in vec4 position;
in vec2 textureCoord;
out vec2 textureCoordFromVShader;
in vec4 color;
out vec4 colorFromVShader;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

void main() {
	gl_Position = Projection * View * Model * position;
	textureCoordFromVShader = textureCoord;
	colorFromVShader = color;
	gl_PointSize = 10.0;
}