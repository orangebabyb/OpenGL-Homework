#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;
uniform float uPixelSize = 64.0;

// --- Add Comparison Uniforms ---
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005; // Optional

void main() {
    // 1. Get original color
    vec4 originalColor = texture(uScreenTexture, vUV);

    // 2. Calculate effect color (Pixelization logic)
    vec2 texSize = textureSize(uScreenTexture, 0);
    float pixelW = uPixelSize / texSize.x;
    float pixelH = uPixelSize / texSize.y;
    float newX = floor(vUV.x / pixelW) * pixelW + 0.5 * pixelW;
    float newY = floor(vUV.y / pixelH) * pixelH + 0.5 * pixelH;
    vec4 effectColor = texture(uScreenTexture, vec2(newX, newY));

    // 3. Apply comparison logic
    vec4 finalColor;
    if (uEnableComparison && vUV.x < uBarPosition) {
        finalColor = originalColor; // Left side shows original
    } else {
        finalColor = effectColor; // Right side shows effect (pixelated)
    }

    // 4. (Optional) Draw the bar itself
    if (uEnableComparison && abs(vUV.x - uBarPosition) < uBarWidth * 0.5) {
        finalColor = vec4(1.0, 0.0, 0.0, 1.0); // Red bar
    }

    FragColor = finalColor;
}