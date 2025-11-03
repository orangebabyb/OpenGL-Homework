#version 410 core
in vec2 vUV;
in vec3 vN; // World-space Normal
out vec4 FragColor;

void main() {
    // 將法線向量 (-1.0 ~ 1.0) 轉換到 (0.0 ~ 1.0) 範圍以便顯示
    vec3 color = normalize(vN) * 0.5 + 0.5;
    FragColor = vec4(color, 1.0);
}