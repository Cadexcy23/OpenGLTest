#version 150

in vec4 colorFromVShader;
in vec2 textureCoordFromVShader;
out vec4 outColor;

uniform sampler2D textureSampler;

void main() {
	//outColor = colorFromVShader;
	outColor = texture(textureSampler, textureCoordFromVShader);

	//mix in colors passed with it
	outColor = texture(textureSampler, textureCoordFromVShader) * colorFromVShader;

	//rand testing stuff
	//outColor = vec4(outColor.r * outColor.a, outColor.g * outColor.a, outColor.b * outColor.a, outColor.a);
	//outColor = vec4(gl_FragCoord.x / 10, gl_FragCoord.y / 10, gl_FragCoord.z / 10, 1.0);

	//grey scale
	//float averageColor = (outColor.r + outColor.g + outColor.b) / 3.0;
	//outColor = vec4(averageColor, averageColor, averageColor, outColor.a);

	//invert colors
	//outColor = vec4(1.0 - outColor.r, 1.0 - outColor.g, 1.0 - outColor.b, outColor.a);

	//clear any fully transparent frags
	//if (outColor.a < 0.05)
	//	discard;
}