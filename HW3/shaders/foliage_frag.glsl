#version 460 core

in vec2 v_UV;
in float v_Layer;
in vec3 v_Normal;
in vec3 v_WorldPos;
in vec3 f_viewVertex;

out vec4 FragColor;

uniform sampler2DArray u_TexArray;

// 相機位置（世界座標，由 C++ 傳入）
uniform vec3 u_CameraPos;

vec4 WithFog(vec4 color){
	const vec4 FOG_COLOR = vec4(0.0, 0.0, 0.0, 1) ;
	const float MAX_DIST = 150.0 ;
	const float MIN_DIST = 120.0 ;
	
	float dis = length(f_viewVertex) ;
	float fogFactor = (MAX_DIST - dis) / (MAX_DIST - MIN_DIST) ;
	fogFactor = clamp(fogFactor, 0.0f, 1.0f) ;
	fogFactor = fogFactor * fogFactor ;
	
	return mix(FOG_COLOR, color, fogFactor) ;
}

void main()
{
    // 取貼圖
    vec4 albedo = texture(u_TexArray, vec3(v_UV, v_Layer));

    // alpha test
    if (albedo.a < 0.5)
        discard;

    // ---------------------------
    // Phong Shading 參數（照投影片）
    // ---------------------------
    // Directional light direction (L)
    vec3 L = normalize(vec3(0.3, 0.7, 0.5));   // 指向「從表面看過去的光線方向」

    // 材質係數
    vec3 Ka = vec3(0.1);
    vec3 Kd = vec3(0.8);
    vec3 Ks = vec3(0.1);

    // 光的顏色/強度
    vec3 I = vec3(1.0);

    // ---------------------------
    // 計算向量
    // ---------------------------
    vec3 N = normalize(v_Normal);                 // 法線
    vec3 V = normalize(u_CameraPos - v_WorldPos); // 往眼睛方向
    vec3 R = reflect(-L, N);                      // 反射方向

    // ---------------------------
    // Phong 公式
    // ---------------------------
    // Ambient
    vec3 ambient = Ka * albedo.rgb * I;

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = Kd * NdotL * albedo.rgb * I;

    // Specular
    float shininess = 1.0;   // 可自己調
    float RdotV = max(dot(R, V), 0.0);
    float specFactor = pow(RdotV, shininess);

    if (NdotL <= 0.0) specFactor = 0.0;

    vec3 specular = Ks * specFactor * I;
    //vec3 specular = vec3(0.0);

    vec3 shadedColor = ambient + diffuse + specular;

    // ---------------------------
    // Reinhard-like tone mapping（你簡報上的公式）
    // mappedColor = 1 - exp(-shadedColor * EXPOSURE)
    // ---------------------------
    const float EXPOSURE = 3.0f;
    vec3 mappedColor = vec3(1.0) - exp(-shadedColor * EXPOSURE);

    vec4 shaded = vec4(mappedColor, albedo.a);
    FragColor = WithFog(shaded);
}
