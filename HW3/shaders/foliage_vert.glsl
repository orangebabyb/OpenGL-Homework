#version 460 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

struct PlantData {
    vec4 positionAndType; 
};

layout(std430, binding = 0) buffer PlantBuffer {
    PlantData plants[];
};

uniform mat4 u_View;
uniform mat4 u_Proj;

out vec2 v_UV;
out float v_Layer;
out vec3 v_Normal;   // [新增] 傳遞法線
out vec3 v_WorldPos; // [新增] 傳遞世界座標
out vec3 f_viewVertex;
void main() {
    uint idx = gl_BaseInstance + gl_InstanceID;
    vec4 data = plants[idx].positionAndType;
    
    vec3 worldPos = data.xyz + a_Pos;
    
    v_UV = a_UV;
    v_Layer = data.w;
    v_Normal = a_Normal; // 假設植物不旋轉，直接用法線
    v_WorldPos = worldPos;
    f_viewVertex = (u_View * vec4(worldPos, 1.0)).xyz;

    gl_Position = u_Proj * u_View * vec4(worldPos, 1.0);
}
