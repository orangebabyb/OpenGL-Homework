#include "Common.h"
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace glm;
using namespace std;

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

int g_sceneRenderMode = 0; // 0: Textured, 1: Normal Color
int g_postEffectMode = 0;  // 0: None, 1: Pixelization, etc.
static float g_BarPosition = 0.5f;
bool g_EnableComparisonBar = false;
bool g_DraggingBar = false;

static float g_SinePower1 = 0.02f;
static float g_SinePower2 = 20.0f;
float g_PixelSize = 16.0f;

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct TextureGL {
	unsigned int id = 0;
	std::string path;
};

static void expandAABB(glm::vec3& bmin, glm::vec3& bmax, const glm::vec3& p) {
    bmin = glm::min(bmin, p);
    bmax = glm::max(bmax, p);
}

static unsigned int compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); fprintf(stderr, "Shader compile error:\n%s\n", log); }
    return s;
}

static std::string loadFileStr(const std::string& p) {
    std::ifstream ifs(p, std::ios::binary);
    std::stringstream ss; ss << ifs.rdbuf(); return ss.str();
}

static unsigned int buildProgramFromFiles(const std::string& vsPath, const std::string& fsPath) {
    std::string v = loadFileStr(vsPath);
    std::string f = loadFileStr(fsPath);
    GLuint vs = compileShader(GL_VERTEX_SHADER, v.c_str());
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, f.c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(prog, 2048, nullptr, log); fprintf(stderr, "Program link error:\n%s\n", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

static unsigned int loadTexture2D(const std::string& file, bool flipY = true) {
    stbi_set_flip_vertically_on_load(flipY);
    int w, h, n; unsigned char* data = stbi_load(file.c_str(), &w, &h, &n, 0);
    if (!data) {
        // fallback: 1x1 白色
        unsigned char white[3] = { 255,255,255 };
        GLuint tex; glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        return tex;
    }
    GLenum format = (n == 1) ? GL_RED : (n == 3 ? GL_RGB : GL_RGBA);
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return tex;
}

struct GLMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    TextureGL diffuse;
    void draw() const {
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuse.id);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
};

static std::string g_dir; // 資料夾路徑前綴
static std::unordered_map<std::string, TextureGL> g_texCache;

static TextureGL loadMaterialTexture(aiMaterial* mat, aiTextureType type) {
    if (mat->GetTextureCount(type) == 0) return {};
    aiString str; mat->GetTexture(type, 0, &str);
    std::string rel = str.C_Str();
    std::string full = g_dir + rel;
    if (g_texCache.count(full)) return g_texCache[full];
    TextureGL t; t.id = loadTexture2D(full); t.path = full;
    g_texCache[full] = t; return t;
}

glm::vec3 lmin(std::numeric_limits<float>::infinity());
glm::vec3 lmax(-std::numeric_limits<float>::infinity());

static GLMesh processMesh(const aiMesh* m, const aiScene* scene, aiMaterial* mat) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(m->mNumVertices);
    for (unsigned i = 0; i < m->mNumVertices; ++i) {
        Vertex v{};
        v.pos = glm::vec3(m->mVertices[i].x, m->mVertices[i].y, m->mVertices[i].z);
        v.normal = m->HasNormals() ? glm::vec3(m->mNormals[i].x, m->mNormals[i].y, m->mNormals[i].z) : glm::vec3(0, 1, 0);
        if (m->mTextureCoords[0]) {
            v.uv = glm::vec2(m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y);
        }
        else v.uv = glm::vec2(0);
        lmin = glm::min(lmin, v.pos);
        lmax = glm::max(lmax, v.pos);
        vertices.push_back(v);
    }
    for (unsigned f = 0; f < m->mNumFaces; ++f) {
        const aiFace& face = m->mFaces[f];
        for (unsigned j = 0; j < face.mNumIndices; ++j) indices.push_back(face.mIndices[j]);
    }
    GLMesh out{};
    glGenVertexArrays(1, &out.vao);
    glGenBuffers(1, &out.vbo);
    glGenBuffers(1, &out.ebo);
    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    out.indexCount = (GLsizei)indices.size();
    // 只取第一張 diffuse（Sponza 多材質，示範先簡化）
    out.diffuse = loadMaterialTexture(mat, aiTextureType_DIFFUSE);
    if (out.diffuse.id == 0) { // 沒 diffuse 就用白貼圖
        out.diffuse.id = loadTexture2D(g_dir + "KAMEN.JPG"); // 或任一存在的圖；不存在也會 fallback
    }
    return out;
}

static void processNode(const aiNode* node, const aiScene* scene, std::vector<GLMesh>& meshes) {
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* m = scene->mMeshes[node->mMeshes[i]];
        aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];
        meshes.emplace_back(processMesh(m, scene, mat));
    }
    for (unsigned i = 0; i < node->mNumChildren; ++i)
        processNode(node->mChildren[i], scene, meshes);
}

struct Model {
    std::vector<GLMesh> meshes;
    glm::vec3 bmin, bmax;  // 物件總 AABB
    bool hasBounds = false;

    void load(const std::string& path) {
        Assimp::Importer importer;
        unsigned flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality | aiProcess_SortByPType |
            aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
            aiProcess_ValidateDataStructure | aiProcess_FlipUVs;
        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || !scene->mRootNode) {
            fprintf(stderr, "Assimp load failed: %s\n", importer.GetErrorString());
            return;
        }
        size_t slash = path.find_last_of("/\\");
        g_dir = (slash == std::string::npos) ? "" : path.substr(0, slash + 1);

        meshes.clear();
        // 初始化 AABB
        bmin = glm::vec3(std::numeric_limits<float>::infinity());
        bmax = glm::vec3(-std::numeric_limits<float>::infinity());
        hasBounds = false;

        // 走訪 node
        std::function<void(const aiNode*)> walk = [&](const aiNode* node) {
            for (unsigned i = 0; i < node->mNumMeshes; ++i) {
                aiMesh* m = scene->mMeshes[node->mMeshes[i]];
                aiMaterial* mat = scene->mMaterials[m->mMaterialIndex];
                // 重複 processMesh 內容中的 lmin/lmax 計算（或把 lmin/lmax 透過回傳帶出來）
                std::vector<Vertex> vertices;
                std::vector<unsigned int> indices;
                vertices.reserve(m->mNumVertices);
                glm::vec3 lmin(std::numeric_limits<float>::infinity());
                glm::vec3 lmax(-std::numeric_limits<float>::infinity());
                for (unsigned vi = 0; vi < m->mNumVertices; ++vi) {
                    Vertex v{};
                    v.pos = glm::vec3(m->mVertices[vi].x, m->mVertices[vi].y, m->mVertices[vi].z);
                    v.normal = m->HasNormals() ? glm::vec3(m->mNormals[vi].x, m->mNormals[vi].y, m->mNormals[vi].z) : glm::vec3(0, 1, 0);
                    if (m->mTextureCoords[0])
                        v.uv = glm::vec2(m->mTextureCoords[0][vi].x, m->mTextureCoords[0][vi].y);
                    else v.uv = glm::vec2(0);
                    vertices.push_back(v);
                    lmin = glm::min(lmin, v.pos);
                    lmax = glm::max(lmax, v.pos);
                }
                for (unsigned f = 0; f < m->mNumFaces; ++f) {
                    const aiFace& face = m->mFaces[f];
                    for (unsigned j = 0; j < face.mNumIndices; ++j) indices.push_back(face.mIndices[j]);
                }
                // 建 GLMesh （跟你原本相同）
                GLMesh out{};
                glGenVertexArrays(1, &out.vao);
                glGenBuffers(1, &out.vbo);
                glGenBuffers(1, &out.ebo);
                glBindVertexArray(out.vao);
                glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
                out.indexCount = (GLsizei)indices.size();
                out.diffuse = loadMaterialTexture(mat, aiTextureType_DIFFUSE);
                if (out.diffuse.id == 0) out.diffuse.id = loadTexture2D(g_dir + "KAMEN.JPG");

                meshes.emplace_back(out);

                // 擴充模型總 AABB
                if (!hasBounds) { bmin = lmin; bmax = lmax; hasBounds = true; }
                else { bmin = glm::min(bmin, lmin); bmax = glm::max(bmax, lmax); }
            }
            for (unsigned c = 0; c < node->mNumChildren; ++c) walk(node->mChildren[c]);
            };
        walk(scene->mRootNode);

        fprintf(stderr, "[Model] meshes=%zu  AABB min(%.2f,%.2f,%.2f) max(%.2f,%.2f,%.2f)\n",
            meshes.size(), bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
    }

    void draw() const { for (auto& m : meshes) m.draw(); }
};


static GLuint gProgram = 0;
static GLuint gProgram_NormalColor = 0;
static GLuint gProgram_Passthrough = 0;
static GLuint gProgram_Pixelate = 0;
static GLuint gProgram_Comparison = 0;
static GLuint gProgram_SineWave = 0;
static GLuint gProgram_Abstraction = 0; 
static GLuint gProgram_Watercolor = 0;  
static GLuint gProgram_Magnifier = 0;
static GLuint gProgram_Bloom = 0;

static GLuint g_NoiseTexture = 0;
static glm::vec2 g_MousePosNormalized = glm::vec2(0.5f);

// (FBO 和 Quad 的變數，你上次已經加過了)
static GLuint fbo = 0;
static GLuint texColorBuffer = 0;
static GLuint rboDepthStencil = 0;
static GLuint quadVAO = 0;
static GLuint quadVBO = 0;

static Model gSponza;
static glm::mat4 gProj, gView, gModel;

struct FPSCamera {
    glm::vec3 pos{ 0.0f, 2.0f, 12.0f };
    float yaw = 3.1415926f;   // 朝向 -Z
    float pitch = 0.0f;
    float speed = 3.0f;         // WASD 速度 (units/sec)
    float sens = 0.0025f;      // 滑鼠靈敏度 (rad/pixel)
    bool dragging = false;
    double lastX = 0.0, lastY = 0.0;
} cam;

static void createFramebuffer(int width, int height) {
    // 如果 FBO 已存在，先刪除舊的
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texColorBuffer);
        glDeleteRenderbuffers(1, &rboDepthStencil);
    }

    // 1. 建立 FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // 2. 建立顏色紋理 (Color Texture)
    glGenTextures(1, &texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

    // 3. 建立深度/模板 Renderbuffer (RBO)
    glGenRenderbuffers(1, &rboDepthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepthStencil);

    // 4. 檢查 FBO 是否完整
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");

    // 5. 解除綁定，回到預設的 Framebuffer (螢幕)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 【新增】視窗大小改變的回呼函數
static void on_framebuffer_size_changed(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    createFramebuffer(width, height); // 重建 FBO
}

static void updateCamera(GLFWwindow* window, float dt) {
    ImGuiIO& io = ImGui::GetIO();

    // 滑鼠左鍵拖曳旋轉視角（避開 ImGui 介面）
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !io.WantCaptureMouse) {
        double x, y; glfwGetCursorPos(window, &x, &y);
        if (!cam.dragging) { cam.dragging = true; cam.lastX = x; cam.lastY = y; }
        double dx = x - cam.lastX, dy = y - cam.lastY;
        cam.lastX = x; cam.lastY = y;
        cam.yaw -= (float)dx * cam.sens;
        cam.pitch -= (float)dy * cam.sens;
        cam.pitch = glm::clamp(cam.pitch, -1.55f, 1.55f); // 夾上下
    }
    else {
        cam.dragging = false;
    }

    // 方向向量
    glm::vec3 forward = glm::normalize(glm::vec3(
        cosf(cam.pitch) * sinf(cam.yaw),
        sinf(cam.pitch),
        cosf(cam.pitch) * cosf(cam.yaw)
    ));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // WASD 移動（Q/E 上下）
    float velocity = cam.speed * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.pos += forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.pos -= forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.pos -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.pos += right * velocity;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) cam.pos -= up * velocity;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) cam.pos += up * velocity;
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height); // 使用視窗大小
    if (width > 0 && height > 0) {
        // 儲存正規化座標 (0,0 在左上角)
        g_MousePosNormalized.x = (float)xpos / (float)width;
        g_MousePosNormalized.y = (float)ypos / (float)height;
    }
}

// 在你的 on_display() 改成支援深度並繪製：
void on_display()
{
    int w, h;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &w, &h);

    // === 更新相機 ===
    static double tPrev = glfwGetTime();
    double tNow = glfwGetTime();
    float dt = float(tNow - tPrev);
    tPrev = tNow;
    updateCamera(glfwGetCurrentContext(), dt);

    // === 建立 P / V 矩陣 ===
    glm::mat4 P = glm::perspective(glm::radians(60.0f), (float)w / (float)h, 0.1f, 5000.0f);
    glm::vec3 forward = glm::normalize(glm::vec3(
        cosf(cam.pitch) * sinf(cam.yaw), sinf(cam.pitch), cosf(cam.pitch) * cosf(cam.yaw)
    ));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    glm::mat4 V = glm::lookAt(cam.pos, cam.pos + forward, up);

    // ------------------------------------------
    // --- Pass 1: Scene Pass (渲染到 FBO) ---
    // ------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 清除 FBO

    GLuint sceneShader;
    if (g_sceneRenderMode == 0) {
        sceneShader = gProgram;
    }
    else {
        sceneShader = gProgram_NormalColor;
    }

    if (!sceneShader || gSponza.meshes.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glUseProgram(sceneShader);

    glUniformMatrix4fv(glGetUniformLocation(sceneShader, "uP"), 1, GL_FALSE, &P[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(sceneShader, "uV"), 1, GL_FALSE, &V[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(sceneShader, "uM"), 1, GL_FALSE, &gModel[0][0]);
    if (g_sceneRenderMode == 0) {
        glUniform1i(glGetUniformLocation(sceneShader, "uTex0"), 0);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gSponza.draw();

    // ------------------------------------------------
    // --- Pass 2: Post-Process Pass (渲染到螢幕) ---
    // ------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint ppShader = gProgram_Passthrough; //Default
    switch (g_postEffectMode) {
        case 1: 
            ppShader = gProgram_Abstraction;
            break;
        case 2: 
            ppShader = gProgram_Watercolor; 
            break;
        case 3: 
            ppShader = gProgram_Magnifier;
            break;
        case 4: 
            ppShader = gProgram_Bloom; 
            break;
        case 5:
            ppShader = gProgram_Pixelate;
            break;
        case 6:
            ppShader = gProgram_SineWave;
            break;
        default:
            ppShader = gProgram_Passthrough;
            break;
    }

    if (!ppShader) return;
    glUseProgram(ppShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    glUniform1i(glGetUniformLocation(ppShader, "uScreenTexture"), 0);
    if (g_postEffectMode == 3) { // Magnifier
        glUniform2f(glGetUniformLocation(ppShader, "uMousePos"),
            g_MousePosNormalized.x,
            1.0f - g_MousePosNormalized.y); // <-- 翻轉 Y
        glUniform1f(glGetUniformLocation(ppShader, "uRadius"), 0.2f);
        glUniform1f(glGetUniformLocation(ppShader, "uZoom"), 2.0f);
    }
    else if (g_postEffectMode == 5) { // Pixelization
        // 【修改】傳送固定值
        glUniform1f(glGetUniformLocation(ppShader, "uPixelSize"), 16.0f);
    }
    else if (g_postEffectMode == 6) { // Sine Wave
        glUniform1f(glGetUniformLocation(ppShader, "uTime"), (float)glfwGetTime()); // 取得目前時間
        // 【修改】傳送固定值 (對應 PDF 的 power1, power2)
        glUniform1f(glGetUniformLocation(ppShader, "uPower1"), 0.02f);
        glUniform1f(glGetUniformLocation(ppShader, "uPower2"), 20.0f);
    }

    glUniform1i(glGetUniformLocation(ppShader, "uEnableComparison"), g_EnableComparisonBar ? 1 : 0); // Send bool as int
    glUniform1f(glGetUniformLocation(ppShader, "uBarPosition"), g_BarPosition);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Unbind noise texture (good practice)
    if (g_postEffectMode == 2 && g_NoiseTexture != 0) { // Watercolor noise
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_NoiseTexture);
        glUniform1i(glGetUniformLocation(ppShader, "uNoiseTexture"), 1);
    }
}

void on_gui()
{
    // Reference: https://raw.githubusercontent.com/ocornut/imgui/refs/heads/master/examples/example_glfw_opengl3/main.cpp
    // Our state
    static bool show_demo_window = true;
    static bool show_another_window = false;

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        // ImGuiCond_FirstUseEver 參數代表「只在第一次執行時設定」，之後使用者可以自由縮放
        ImGui::SetNextWindowSize(ImVec2(1000, 500), ImGuiCond_FirstUseEver);

        ImGui::Begin("Hello, world!");                  // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");         // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Separator(); // 加個分隔線讓 UI 清楚一點

        // 【已移動】將右鍵選單的內容移到這裡
        // 作業要求 有「Textured scene + Normal Color」
        ImGui::Text("Scene Render Mode");
        ImGui::RadioButton("Textured", &g_sceneRenderMode, 0);
        ImGui::RadioButton("Normal Color", &g_sceneRenderMode, 1);

        ImGui::Separator(); // 分隔線

        // 作業要求的各種後製效果
        ImGui::Text("Post-Processing Effect");
        ImGui::RadioButton("None", &g_postEffectMode, 0);
        ImGui::RadioButton("Image Abstraction", &g_postEffectMode, 1);
        ImGui::RadioButton("Watercolor", &g_postEffectMode, 2);
        ImGui::RadioButton("Magnifier", &g_postEffectMode, 3);
        ImGui::RadioButton("Bloom Effect", &g_postEffectMode, 4);
        ImGui::RadioButton("Pixelization", &g_postEffectMode, 5);
        ImGui::RadioButton("Sine Wave", &g_postEffectMode, 6);

        // 作業要求的「Comparison Bar」
        ImGui::Separator();
        ImGui::Checkbox("Enable Comparison Bar", &g_EnableComparisonBar);
        // 【新增】Only show the slider if the checkbox is checked
        if (g_EnableComparisonBar) {
            ImGui::SameLine(); // Place slider next to checkbox
            ImGui::SliderFloat("Bar Position", &g_BarPosition, 0.0f, 1.0f);
        }

        ImGui::Separator(); // 再加個分隔線

        //Adjust Camera position and lookat
        ImGui::Text("Camera Position");
        ImGui::DragFloat3("Pos", (float*)&cam.pos, 0.1f);
        ImGui::SliderFloat("Yaw", &cam.yaw, -3.14f, 3.14f);
        ImGui::SliderFloat("Pitch", &cam.pitch, -1.5f, 1.5f);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    //3. Show another simple window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**)
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;

	const char* glsl_version = "#version 410";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);


	// Create window with graphics context
	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "CG Template (with Imgui)", nullptr, nullptr);
	if (window == nullptr) return 1;
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD\n";
		return -1;
	}

	glfwSwapInterval(1); // Enable vsync
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size_changed);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // ====== 【在此區塊插入初始化：GL 開啟深度、建 shader、載 Sponza、設矩陣】 ======
    glEnable(GL_DEPTH_TEST);

    ////Pass 1 
    gProgram = buildProgramFromFiles("shaders/scene_vs.glsl", "shaders/scene_fs.glsl");
    gProgram_NormalColor = buildProgramFromFiles("shaders/scene_vs.glsl", "shaders/scene_fs_normal.glsl");

    //Pass2
    gProgram_Passthrough = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_passthrough.glsl");
    gProgram_Pixelate = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_pixelate.glsl");
    gProgram_SineWave = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_sinewave.glsl");
    gProgram_Abstraction = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_abstraction.glsl");
    gProgram_Watercolor = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_watercolor.glsl");
    gProgram_Magnifier = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_magnifier.glsl");
    gProgram_Bloom = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_bloom.glsl");

    //comparison bar
    gProgram_Comparison = buildProgramFromFiles("shaders/pp_vs.glsl", "shaders/pp_fs_comparison.glsl");
    
    // Ensure you have an 'assets/noise.png' file!
    g_NoiseTexture = loadTexture2D("assets/123.png", false);

    // 載入 Sponza（依你的實際路徑）
    gSponza.load("assets/sponza/sponza.obj");

    // 自動置中/縮放/相機距離
    if (gSponza.hasBounds) {
        glm::vec3 center = 0.5f * (gSponza.bmin + gSponza.bmax);
        float radius = 0.5f * glm::length(gSponza.bmax - gSponza.bmin);
        if (radius < 1e-5f) radius = 1.0f;

        float targetR = 6.0f;                     // 想更鬆/更滿畫面可調
        float s = targetR / radius;

        // 模型：先移到原點再縮放
        gModel = glm::scale(glm::translate(glm::mat4(1.0f), -center), glm::vec3(s));

        // === 直接指定相機起始狀態（你要的數值） ===
        cam.pos = glm::vec3(-2.631f, -5.634f, 0.528f);
        cam.yaw = 8.129f;
        cam.pitch = 0.093f;

        // 用 yaw/pitch 算 forward，再建 gView
        glm::vec3 fwd = glm::normalize(glm::vec3(
            cosf(cam.pitch) * sinf(cam.yaw),
            sinf(cam.pitch),
            cosf(cam.pitch) * cosf(cam.yaw)
        ));
        gView = glm::lookAt(cam.pos, cam.pos + fwd, glm::vec3(0, 1, 0));

        fprintf(stderr, "[InitCam] pos(%.3f,%.3f,%.3f) yaw=%.3f pitch=%.3f\n",
            cam.pos.x, cam.pos.y, cam.pos.z, cam.yaw, cam.pitch);
    }

    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // vPos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // vUV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0); // 解除綁定

    // 基本相機矩陣
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    gProj = glm::perspective(glm::radians(20.0f), float(fbw) / float(fbh), 0.1f, 5000.0f);
    gView = glm::lookAt(glm::vec3(0.0f, 2.0f, 5.0f),
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(0, 1, 0));
    //gModel = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
    // ====== 【到此為止】 ======================================================
    
    createFramebuffer(fbw, fbh);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);

	ImGui_ImplOpenGL3_Init(glsl_version);

	// Main loop
	while (!glfwWindowShouldClose(window))

	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();
		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		on_gui();
		// Rendering
		on_display();
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}