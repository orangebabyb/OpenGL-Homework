#version 410 core
in vec2 vUV;
in vec3 vN; // 這是 World-space Normal
out vec4 FragColor;

uniform sampler2D uTex0;
uniform vec3 uLightDir = normalize(vec3(-0.5, -1.0, -0.3));
uniform float uAmbient = 0.25;

void main() {
    vec3 albedo = texture(uTex0, vUV).rgb;
    vec3 N_norm = normalize(vN); // 法線已經在 world-space
    float diff = max(dot(N_norm, -uLightDir), 0.0);
    vec3 color = albedo * (uAmbient + (1.0 - uAmbient) * diff);
    FragColor = vec4(color, 1.0);
}