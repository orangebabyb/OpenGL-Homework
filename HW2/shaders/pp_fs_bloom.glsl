#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;

// Comparison Uniforms
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005;

// --- Bloom Parameters (Hardcoded) ---
const float bloomThreshold = 0.75; // 亮度閾值
const float bloomIntensity = 3.5; // 光暈強度 (可調高)
const int blurRadius = 7; // 模糊採樣半徑 (例如 7x7 範圍)

// --- 高斯模糊權重 (近似值，可以預先計算好) ---
// 這裡用一個簡單的線性衰減代替，效果接近但計算簡單
// const float gaussianWeight[blurRadius + 1] = float[]( ... ); // 精確高斯權重

// Extract bright parts (Thresholding) - 使用 smoothstep 平滑過渡
vec4 extractBright(vec4 color) {
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    float factor = smoothstep(bloomThreshold - 0.1, bloomThreshold + 0.15, brightness); // 稍微放寬範圍
    return color * factor;
}

// 近似高斯模糊 (合併水平和垂直)
vec4 applyApproxGaussian(sampler2D tex, vec2 uv) {
    vec2 texelSize = 1.0 / textureSize(tex, 0); // 單個像素在 UV 中的大小
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    for (int x = -blurRadius; x <= blurRadius; ++x) {
        for (int y = -blurRadius; y <= blurRadius; ++y) {
            // 計算距離中心的權重 (簡單線性衰減)
            float dist = sqrt(float(x*x + y*y));
            float weight = max(0.0, 1.0 - dist / float(blurRadius)); // 距離越遠權重越低
            
            // 計算採樣座標
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // 只對亮部進行採樣和加權
            result += extractBright(texture(tex, uv + offset)) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0) {
        return result / totalWeight; // 平均權重
    } else {
        return vec4(0.0);
    }
}


void main() {
    vec4 originalColor = texture(uScreenTexture, vUV);

    // Calculate effect color: Original + Blurred Bright Parts
    // 【修改】使用新的模糊函數
    vec4 blurredBright = applyApproxGaussian(uScreenTexture, vUV); 
    vec4 effectColor = originalColor + blurredBright * bloomIntensity;

    // Apply Comparison Bar logic
    vec4 finalColor;
    if (uEnableComparison && vUV.x < uBarPosition) {
        finalColor = originalColor;
    } else {
        finalColor = effectColor;
    }
    if (uEnableComparison && abs(vUV.x - uBarPosition) < uBarWidth * 0.5) {
        finalColor = vec4(1.0, 0.0, 0.0, 1.0); // Red bar
    }
    
    // Tone Mapping (可選，避免顏色過曝)
    // finalColor.rgb = finalColor.rgb / (finalColor.rgb + vec3(1.0)); // Reinhard tone mapping
    // finalColor.rgb = pow(finalColor.rgb, vec3(1.0/2.2)); // Gamma correction 

    FragColor = finalColor;
}