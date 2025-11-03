#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;

// --- Add Comparison Uniforms ---
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005; // Optional: for drawing the bar

void main() {
    // 1. Get original color
    vec4 originalColor = texture(uScreenTexture, vUV);

    // 2. Calculate "effect" color (for passthrough, it's just the original)
    vec4 effectColor = originalColor; 

    // 3. Apply comparison logic
    vec4 finalColor;
    if (uEnableComparison && vUV.x < uBarPosition) {
        finalColor = originalColor; // Left side shows original
    } else {
        finalColor = effectColor; // Right side shows effect
    }

    // 4. (Optional) Draw the bar itself
    if (uEnableComparison && abs(vUV.x - uBarPosition) < uBarWidth * 0.5) {
        finalColor = vec4(1.0, 0.0, 0.0, 1.0); // Red bar
    }
    
    FragColor = finalColor;
}