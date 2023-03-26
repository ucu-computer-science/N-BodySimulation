#version 430 core

in vec3 Color;
in vec2 TexCoord;
out vec4 FragColor;

uniform float ColorScale;
uniform sampler2D ourTexture;

void main()
{
	FragColor = texture(ourTexture, TexCoord) * vec4(Color*ColorScale, 1.0);
}
