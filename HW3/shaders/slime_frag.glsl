#version 460 core
in vec2 v_UV;
in vec3 v_Normal;
in vec3 v_WorldPos;

out vec4 FragColor;

uniform sampler2D u_Tex;
uniform vec3 u_ViewPos;      // 相機世界座標

void main() {
    vec4 albedo = texture(u_Tex, v_UV);
    if (albedo.a < 0.3) discard;  // 有需要可以保留 alpha test

    // --- Phong lighting parameters (照 slide) ---
    const vec3 Ldir = normalize(vec3(0.3, 0.7, 0.5));     // directional light
    const vec3 Ka   = vec3(0.1, 0.1, 0.1);
    const vec3 Kd   = vec3(0.8, 0.8, 0.8);
    const vec3 Ks   = vec3(0.1, 0.1, 0.1);
    const float shininess = 32.0;

    vec3 N = normalize(v_Normal);
    vec3 L = Ldir;
    vec3 V = normalize(u_ViewPos - v_WorldPos);
    vec3 R = reflect(-L, N);

    // Ambient + diffuse + specular
    float diff = max(dot(N, L), 0.0);
    float spec = 0.0;
    if (diff > 0.0)
        spec = pow(max(dot(R, V), 0.0), shininess);

    vec3 ambient  = Ka * albedo.rgb;
    vec3 diffuse  = Kd * diff * albedo.rgb;
    vec3 specular = Ks * spec;          // specular 不乘 albedo 比較像金屬高光

    vec3 shadedColor = ambient + diffuse + specular;

    // --- Reinhard tone mapping (照你的投影片) ---
    const float EXPOSURE = 3.0;
    vec3 mappedColor = vec3(1.0) - exp(-shadedColor * EXPOSURE);

    FragColor = vec4(mappedColor, albedo.a);
}