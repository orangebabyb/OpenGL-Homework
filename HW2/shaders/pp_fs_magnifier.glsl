#version 410 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uScreenTexture;

// --- Magnifier Uniforms ---
uniform vec2 uMousePos;   // Mouse position (0.0 to 1.0, Y flipped)
uniform float uRadius = 0.2; // Magnifier radius (normalized)
uniform float uZoom = 2.0;   // Zoom factor

// --- Comparison Uniforms ---
uniform bool uEnableComparison;
uniform float uBarPosition;
uniform float uBarWidth = 0.005;

void main() {
    vec4 originalColor = texture(uScreenTexture, vUV);
    vec4 effectColor = originalColor; // Default to original

    // Calculate distance from mouse
    float dist = distance(vUV, uMousePos);

    // If inside the radius, calculate zoomed UV
    if (dist < uRadius) {
        vec2 relativeUV = (vUV - uMousePos) / uRadius; 
        relativeUV /= uZoom;
        vec2 zoomedUV = uMousePos + relativeUV * uRadius;

        // Sample texture with zoomed UV (clamp to avoid sampling outside)
        if (zoomedUV.x >= 0.0 && zoomedUV.x <= 1.0 && zoomedUV.y >= 0.0 && zoomedUV.y <= 1.0) {
             effectColor = texture(uScreenTexture, zoomedUV);
        } else {
             effectColor = vec4(0.0); // Black outside texture bounds
        }
    }

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