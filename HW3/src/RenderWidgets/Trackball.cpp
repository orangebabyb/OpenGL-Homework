#include "Trackball.h"
#include <algorithm>
#include <cmath>

namespace INANOA {

    Trackball::Trackball() 
        : m_width(1344), m_height(756), 
          m_eye(0.0f), m_center(0.0f), m_up(0.0f, 1.0f, 0.0f),
          m_isRotating(false), m_isPanning(false)
    {
    }

    Trackball::~Trackball() {}

    void Trackball::reset(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
        m_eye = eye;
        m_center = center;
        m_up = up;
    }

    void Trackball::resize(int w, int h) {
        m_width = w;
        m_height = h;
    }

    // 輔助函式：將 2D 螢幕座標映射到 3D Arcball 球面
    glm::vec3 Trackball::mapToSphere(float x, float y) {
        // 正規化座標到 [-1, 1]
        float sx = (2.0f * x - m_width) / m_width;
        float sy = (m_height - 2.0f * y) / m_height; // Y 軸反轉

        glm::vec3 p(sx, sy, 0.0f);
        float lenSq = sx * sx + sy * sy;

        // 如果點在球內，計算 Z；如果在球外，正規化
        if (lenSq <= 1.0f) {
            p.z = std::sqrt(1.0f - lenSq);  //畢氏定理算出球面 Z
        } else {
            p = glm::normalize(p);
        }
        return p;
    }

    // ==========================
    // 旋轉 (Rotate)
    // ==========================
    // 修改 beginRotate
    void Trackball::beginRotate(float x, float y) {
        // 不再使用 mapToSphere，改用螢幕座標
        m_startPanPos = glm::vec3(x, y, 0.0f); // 借用 m_startPanPos 來存
        m_isRotating = true;
    }

    // 請替換原本的 rotate 函式
    void Trackball::rotate(float x, float y) {
        if (!m_isRotating) return;

        // 計算滑鼠位移量
        float dx = x - m_startPanPos.x; // 借用 m_startPanPos 的 x,y 紀錄上一幀位置
        float dy = y - m_startPanPos.y;
        
        // 如果位移很小則忽略
        if (abs(dx) < 0.1f && abs(dy) < 0.1f) return;

        // [參數] 旋轉靈敏度 (調小這裡可以更慢)
        float sensitivity = 0.005f; 

        // 1. 計算目前的 View Vector
        glm::vec3 view = m_eye - m_center;
        
        // 2. 水平旋轉 (沿著世界座標 Y 軸)
        // 負號是為了讓滑鼠向左拖時，相機向左轉(看右邊)
        float angleY = -dx * sensitivity; 
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), angleY, glm::vec3(0.0f, 1.0f, 0.0f));
        view = glm::vec3(rotY * glm::vec4(view, 0.0f));

        // 3. 垂直旋轉 (沿著相機的 Right 向量)
        // 計算目前的 Right 向量
        glm::vec3 right = glm::normalize(glm::cross(view, glm::vec3(0.0f, 1.0f, 0.0f)));
        float angleX = -dy * sensitivity;
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), angleX, right);
        
        // 預先計算旋轉後的向量，避免轉過頭(例如越過頭頂)造成翻轉
        glm::vec3 newView = glm::vec3(rotX * glm::vec4(view, 0.0f));
        
        // 檢查是否與 Up 向量太接近 (限制俯仰角，避免萬向鎖)
        float dot = glm::dot(glm::normalize(newView), glm::vec3(0.0f, 1.0f, 0.0f));
        if (abs(dot) < 0.99f) {
            view = newView;
        }

        // 4. 更新 Eye 位置與 Up 向量
        m_eye = m_center + view;
        m_up = glm::vec3(0.0f, 1.0f, 0.0f); // 強制鎖定 Up 為 Y 軸，避免場景歪斜

        // 更新滑鼠起始位置 (這很重要，為了計算差值)
        m_startPanPos = glm::vec3(x, y, 0.0f); 
    }

    // ==========================
    // 平移 (Pan)
    // ==========================
    void Trackball::beginPan(float x, float y) {
        m_startPanPos = glm::vec3(x, y, 0.0f);
        m_isPanning = true;
    }

    void Trackball::pan(float x, float y) {
        if (!m_isPanning) return;

        float dx = x - m_startPanPos.x;
        float dy = y - m_startPanPos.y;

        // 計算相機座標系的 Right 和 Up 向量
        glm::vec3 forward = glm::normalize(m_center - m_eye);
        glm::vec3 right = glm::normalize(glm::cross(forward, m_up));
        glm::vec3 camUp = glm::normalize(glm::cross(right, forward));

        // 根據螢幕移動量計算世界座標移動量 (係數 0.05f 可依需求調整靈敏度)
        float sensitivity = 0.01f;
        glm::vec3 translation = (right * -dx + camUp * dy) * sensitivity;

        // 同時移動 Eye 和 Center
        m_eye += translation;
        m_center += translation;

        // 更新起始點
        m_startPanPos = glm::vec3(x, y, 0.0f);
    }
}