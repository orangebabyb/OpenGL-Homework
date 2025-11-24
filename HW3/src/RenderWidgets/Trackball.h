#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace INANOA {

    class Trackball {
    public:
        Trackball();
        ~Trackball();

        // 初始化與重置
        void reset(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);
        
        // 設定視窗大小 (計算滑鼠座標投影用)
        void resize(int w, int h);

        // 滑鼠操作介面
        void beginRotate(float x, float y);
        void rotate(float x, float y);

        void beginPan(float x, float y);
        void pan(float x, float y);

        // 取得相機矩陣參數
        glm::vec3 getEye() const { return m_eye; }
        glm::vec3 getCenter() const { return m_center; }
        glm::vec3 getUp() const { return m_up; }

    private:
        // 將螢幕座標 (x,y) 映射到單位球面上
        glm::vec3 mapToSphere(float x, float y);

    private:
        int m_width;
        int m_height;

        // 相機參數
        glm::vec3 m_eye;
        glm::vec3 m_center;
        glm::vec3 m_up;

        // 操作狀態
        glm::vec3 m_startPosVector; // 旋轉起始點 (球面上)
        glm::vec3 m_startPanPos;    // 平移起始點 (螢幕座標)
        
        bool m_isRotating;
        bool m_isPanning;
    };
}