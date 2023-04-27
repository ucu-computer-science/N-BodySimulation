#version 430 core

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Color; 
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec2 a_ID;

uniform mat4x4 u_ProjView;
uniform mat4x4 u_Model;
uniform mat4x4 u_CameraRotation;
uniform sampler2D u_TexturePos;

out vec3 v_Color;
out vec2 v_TexCoord;

void main()
{
	vec4 translation = texture(u_TexturePos, a_ID);
	vec4 position = u_CameraRotation * vec4(a_Pos, 0.2) + translation;
	gl_Position = u_ProjView * position;
	v_Color	= a_Color;
	v_TexCoord = a_TexCoord;
}