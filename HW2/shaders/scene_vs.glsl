#version 410 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV_in;

// 來自 main.cpp 的 Uniforms
uniform mat4 uP;
uniform mat4 uV;
uniform mat4 uM;

// 傳遞給 Fragment Shader 的變數
out vec3 vN;     // World-space Normal
out vec2 vUV;

void main()
{
    // 轉換頂點到 World-space
    vec4 worldPos = uM * vec4(vPos, 1.0);
    
    // 轉換法線到 World-space (假設 uM 沒有 non-uniform scaling)
    // 嚴謹的作法是用 (inverse transpose of uM)
    vN = normalize(mat3(uM) * vNormal); 
    
    // 傳遞 UV
    vUV = vUV_in;

    // 計算最終的 Clip-space 位置
    gl_Position = uP * uV * worldPos;
}