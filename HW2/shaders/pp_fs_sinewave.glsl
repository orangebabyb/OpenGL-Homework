#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture; // FBO 紋理

// --- Sine Wave Uniform ---
uniform float uTime;      // 目前時間 (用於動畫)
// power1 和 power2 的值將從 C++ 固定傳入

// --- Comparison Uniforms (複製過來的) ---
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005;

// --- 硬編碼的 Power 值 ---
const float power1 = 0.02; // 直接寫在 shader 裡
const float power2 = 20.0; // 直接寫在 shader 裡
const float PI = 3.14159265;

void main() {
    // 1. 計算原始顏色 (Comparison 需要)
    vec4 originalColor = texture(uScreenTexture, vUV);

    // 2. 計算 Sine Wave 效果
    vec2 distortedUV = vUV;
    float offset = uTime; // PDF 上的 offset 就是時間
    // 根據 PDF 公式修改 x 座標
    distortedUV.x += power1 * sin(vUV.y * power2 * PI + offset); 

    vec4 effectColor;
    // 檢查扭曲後的 UV 是否還在 0.0 ~ 1.0 範圍內
    if (distortedUV.x >= 0.0 && distortedUV.x <= 1.0 && distortedUV.y >= 0.0 && distortedUV.y <= 1.0) {
         effectColor = texture(uScreenTexture, distortedUV);
    } else {
         effectColor = vec4(0.0, 0.0, 0.0, 1.0); // 超出範圍顯示黑色
    }

    // 3. 應用 Comparison Bar 邏輯
    vec4 finalColor;
    if (uEnableComparison && vUV.x < uBarPosition) {
        finalColor = originalColor;
    } else {
        finalColor = effectColor;
    }

    // 4. (可選) 畫出紅色的 Bar
    if (uEnableComparison && abs(vUV.x - uBarPosition) < uBarWidth * 0.5) {
        finalColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    
    FragColor = finalColor;
}