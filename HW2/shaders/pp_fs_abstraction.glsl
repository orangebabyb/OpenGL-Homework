#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;

// Comparison Uniforms
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005;

// --- Abstraction Parameters (Hardcoded) ---
const float blurSize = 1.0 / 512.0; // Adjust blur radius based on texture size
const float numColorLevels = 8.0;   // Number of quantization levels
const float edgeThreshold = 0.2;    // Edge detection sensitivity

// Simple Box Blur
vec4 applyBlur(sampler2D tex, vec2 uv) {
    vec4 sum = vec4(0.0);
    sum += texture(tex, uv + vec2(-blurSize, -blurSize));
    sum += texture(tex, uv + vec2( 0.0,     -blurSize));
    sum += texture(tex, uv + vec2( blurSize, -blurSize));
    sum += texture(tex, uv + vec2(-blurSize,  0.0));
    sum += texture(tex, uv + vec2( 0.0,      0.0));
    sum += texture(tex, uv + vec2( blurSize,  0.0));
    sum += texture(tex, uv + vec2(-blurSize,  blurSize));
    sum += texture(tex, uv + vec2( 0.0,      blurSize));
    sum += texture(tex, uv + vec2( blurSize,  blurSize));
    return sum / 9.0;
}

// Sobel Edge Detection (approximates DoG effect [cite: 78])
float applyEdgeDetection(sampler2D tex, vec2 uv) {
    float dx = blurSize; // Use blurSize for pixel offset
    float dy = blurSize;
    vec3 tl = texture(tex, uv + vec2(-dx, -dy)).rgb;
    vec3 tm = texture(tex, uv + vec2(0.0,-dy)).rgb;
    vec3 tr = texture(tex, uv + vec2( dx, -dy)).rgb;
    vec3 ml = texture(tex, uv + vec2(-dx, 0.0)).rgb;
    vec3 mr = texture(tex, uv + vec2( dx, 0.0)).rgb;
    vec3 bl = texture(tex, uv + vec2(-dx,  dy)).rgb;
    vec3 bm = texture(tex, uv + vec2(0.0,  dy)).rgb;
    vec3 br = texture(tex, uv + vec2( dx,  dy)).rgb;

    vec3 gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    vec3 gy = -tl - 2.0*tm - tr + bl + 2.0*bm + br;
    float edge = length(gx) + length(gy);
    return smoothstep(0.0, edgeThreshold, edge); // Make edges black
}

// Color Quantization [cite: 77]
vec3 applyQuantization(vec3 color) {
    return floor(color * numColorLevels) / numColorLevels;
}

void main() {
    vec4 originalColor = texture(uScreenTexture, vUV);

    // Calculate effect color: Blur + Quantization + Edge Detection [cite: 76, 79]
    vec4 blurredColor = applyBlur(uScreenTexture, vUV);
    vec3 quantizedColor = applyQuantization(blurredColor.rgb);
    float edgeFactor = applyEdgeDetection(uScreenTexture, vUV); // Use original for edges
    vec3 effectRgb = mix(quantizedColor, vec3(0.0), edgeFactor); // Make edges black
    vec4 effectColor = vec4(effectRgb, 1.0);

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