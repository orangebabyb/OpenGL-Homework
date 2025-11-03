#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture; // FBO 原始紋理
uniform float uBarPosition = 0.5; // 拉動條位置 (0.0 ~ 1.0)
uniform float uBarWidth = 0.005;  // 拉動條本身的寬度 (可選)

// --- 加入控制比較的 Uniform ---
uniform int uCompareEffectType = 5; // 5=Pixelate, 6=Sine Wave, etc. (0 or default = None)

// --- 加入所有可能需要的 Uniforms ---
uniform float uPixelSize = 64.0;
uniform float uTime = 0.0;       // For Sine Wave
uniform float uSinePower1 = 0.02; // For Sine Wave
uniform float uSinePower2 = 20.0; // For Sine Wave
// (之後加入 Watercolor, Bloom 等需要的 uniforms...)


[cite_start]// Helper function for Sine Wave (從 PDF [cite: 168])
vec2 calculateSineWaveUV(vec2 uv, float time, float power1, float power2) {
    float offset = time;
    uv.x += power1 * sin(uv.y * power2 * 3.14159 + offset);
    return uv;
}


void main() {
    // 1. 計算原始顏色
    vec4 originalColor = texture(uScreenTexture, vUV);

    // 2. 根據 uCompareEffectType 計算效果顏色
    vec4 effectColor = originalColor; // 預設為原始顏色

    if (uCompareEffectType == 5) { // Pixelization
        vec2 texSize = textureSize(uScreenTexture, 0);
        float pixelW = uPixelSize / texSize.x;
        float pixelH = uPixelSize / texSize.y;
        float newX = floor(vUV.x / pixelW) * pixelW + 0.5 * pixelW;
        float newY = floor(vUV.y / pixelH) * pixelH + 0.5 * pixelH;
        effectColor = texture(uScreenTexture, vec2(newX, newY));
    } 
    else if (uCompareEffectType == 6) { // Sine Wave
        vec2 sineUV = calculateSineWaveUV(vUV, uTime, uSinePower1, uSinePower2);
        // 確保 UV 不會超出範圍 (sine 可能會把它推出去)
        if (sineUV.x >= 0.0 && sineUV.x <= 1.0 && sineUV.y >= 0.0 && sineUV.y <= 1.0) {
             effectColor = texture(uScreenTexture, sineUV);
        } else {
             effectColor = vec4(0.0, 0.0, 0.0, 1.0); // 超出範圍顯示黑色
        }
    }
    // else if (uCompareEffectType == 2) { // Watercolor }
    // ... 其他效果 ...

    // 3. 根據 uBarPosition 混合
    float mixFactor = step(uBarPosition, vUV.x); 
    FragColor = mix(originalColor, effectColor, mixFactor);

    // 4. (可選) 畫出拉動條本身
    if (abs(vUV.x - uBarPosition) < uBarWidth * 0.5) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // 紅色
    }
}