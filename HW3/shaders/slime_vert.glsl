#version 460 core
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

uniform mat4 u_Proj;
uniform mat4 u_View;
uniform mat4 u_Model;        // 史萊姆 model matrix

out vec2 v_UV;
out vec3 v_Normal;
out vec3 v_WorldPos;

void main() {
    // 世界座標
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0);
    v_WorldPos = worldPos.xyz;

    // 法線用 normal matrix 變換到世界空間
    mat3 normalMat = transpose(inverse(mat3(u_Model)));
    v_Normal = normalize(normalMat * a_Normal);

    v_UV = a_UV;

    gl_Position = u_Proj * u_View * worldPos;
}
