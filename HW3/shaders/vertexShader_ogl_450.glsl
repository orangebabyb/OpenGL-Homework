#version 450 core

layout(location=0) in vec3 v_vertex;

out vec3 f_worldVertex;
out vec3 f_viewVertex;

layout(location = 0) uniform mat4 modelMat ;
layout(location = 1) uniform mat4 viewMat ;
layout(location = 2) uniform mat4 projMat ;

void main(){
	vec4 worldVertex = modelMat * vec4(v_vertex, 1.0);
	vec4 viewVertex = viewMat * worldVertex;
	
	f_worldVertex = worldVertex.xyz;
	f_viewVertex = viewVertex.xyz;
	
	gl_Position = projMat * viewVertex;
}