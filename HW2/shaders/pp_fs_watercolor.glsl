#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;
uniform sampler2D uNoiseTexture; // Noise texture [cite: 95, 108]

// Comparison Uniforms
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005;

// --- Watercolor Parameters (Hardcoded) ---
const float blurSizeWC = 1.0 / 256.0; // Larger blur for watercolor
const float noiseStrength = 0.02;    // How much noise distorts UVs [cite: 99]
const float numColorLevelsWC = 6.0;  // Quantization levels [cite: 101]

// Simple Box Blur
vec4 applyBlurWC(sampler2D tex, vec2 uv) {
    vec4 sum = vec4(0.0);
    // (Similar to abstraction blur, but maybe use blurSizeWC)
    sum += texture(tex, uv + vec2(-blurSizeWC, -blurSizeWC));
    sum += texture(tex, uv + vec2( 0.0,       -blurSizeWC));
    sum += texture(tex, uv + vec2( blurSizeWC, -blurSizeWC));
    sum += texture(tex, uv + vec2(-blurSizeWC,  0.0));
    sum += texture(tex, uv + vec2( 0.0,        0.0));
    sum += texture(tex, uv + vec2( blurSizeWC,  0.0));
    sum += texture(tex, uv + vec2(-blurSizeWC,  blurSizeWC));
    sum += texture(tex, uv + vec2( 0.0,        blurSizeWC));
    sum += texture(tex, uv + vec2( blurSizeWC,  blurSizeWC));
    return sum / 9.0;
}

// Color Quantization
vec3 applyQuantizationWC(vec3 color) {
    return floor(color * numColorLevelsWC) / numColorLevelsWC;
}

void main() {
    vec4 originalColor = texture(uScreenTexture, vUV);

    // 1. Blur the original image [cite: 96]
    vec4 blurredColor = applyBlurWC(uScreenTexture, vUV);

    // 2. Get noise and distort UVs [cite: 97, 98]
    vec4 noise = texture(uNoiseTexture, vUV * 2.0); // Sample noise (scale UV for frequency)
    // Use noise to offset the UV coordinates for the blurred image
    vec2 distortedUV = vUV + (noise.rg * 2.0 - 1.0) * noiseStrength; 

    // 3. Sample blurred texture with distorted UV [cite: 100]
    vec4 distortedSample = texture(uScreenTexture, distortedUV); // Use original texture for sampling

    // 4. Quantize the result [cite: 101]
    vec3 quantizedColor = applyQuantizationWC(distortedSample.rgb);
    vec4 effectColor = vec4(quantizedColor, 1.0);

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
    FragColor = finalColor;
}