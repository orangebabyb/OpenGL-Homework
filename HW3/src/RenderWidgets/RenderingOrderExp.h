#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include "../Rendering/RendererBase.h"
#include "../Scene/RViewFrustum.h"
#include "../Scene/RHorizonGround.h"
#include "Trackball.h"

#include <glad/glad.h>  
#include <glm/glm.hpp>

#include "../Scene/Trajectory.h" 

// [新增] 定義 GPU 間接繪製指令結構 (必須符合 OpenGL std430 排列)
struct IndirectDrawCmd {
	unsigned int count;         // 頂點數量
	unsigned int instanceCount; // 實例數量
	unsigned int firstIndex;    // Index Buffer 起始點
	unsigned int baseVertex;    // Vertex Buffer 起始點
	unsigned int baseInstance;  // Instance Buffer 起始點
};

// [新增] 定義單個植物的資料結構
struct PlantInstance {
	// 使用 vec4 節省空間：x, y, z 為位置, w 為貼圖/種類 ID (0=草, 1=灌木1, 2=灌木2)
	glm::vec4 positionAndType;
};

// [新增] 簡單的 Mesh 結構來管理 VAO
struct SimpleMesh {
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;
	unsigned int indexCount = 0;
};

namespace INANOA {
	class RenderingOrderExp
	{
	public:
		RenderingOrderExp();
		virtual ~RenderingOrderExp();

	public:
		bool init(const int w, const int h) ;
		void resize(const int w, const int h) ;
		void update() ;
		void render();

		// 在 public 加入：
		void setPlayerMoveState(int dirIndex, bool pressed); // 0:W, 1:S, 2:A, 3:D
		void onGodViewRotateBegin(float x, float y);
		void onGodViewRotate(float x, float y);
		void onGodViewPanBegin(float x, float y);
		void onGodViewPan(float x, float y);
		void onGodViewZoom(float delta);

	private:
		SCENE::RViewFrustum* m_viewFrustum = nullptr;
		SCENE::EXPERIMENTAL::HorizonGround* m_horizontalGround = nullptr;

		Camera* m_playerCamera = nullptr;
		Camera* m_godCamera = nullptr;

		glm::vec3 m_cameraForwardMagnitude;
		float m_cameraForwardSpeed;

		int m_frameWidth;
		int m_frameHeight;

		Trackball m_trackball;
		OPENGL::RendererBase* m_renderer = nullptr;

		// Player state
		bool m_playerInput[4] = { false, false, false, false };
		float m_playerYaw = 0.0f;
		glm::vec3 m_playerPos = glm::vec3(0, 10, 0);

		// ==========================================
		// Phase 2: Resources
		// ==========================================
		SimpleMesh m_meshes[3];
		GLuint m_texArrayHandle = 0;
		std::vector<PlantInstance> m_allInstancesCPU;
		unsigned int m_plantOffsets[3];
		unsigned int m_plantCounts[3];

		bool initResources();
		bool loadOBJ(const std::string& path, SimpleMesh& outMesh);
		GLuint createTextureArray(const std::vector<std::string>& files);
		void loadSpatialSamples();

		// ==========================================
		// Phase 3 & 4: GPU Culling
		// ==========================================
		GLuint m_ssbo_AllPlants = 0;
		GLuint m_ssbo_Indirect = 0;
		GLuint m_programFoliage = 0;

		void createFoliageBuffers();
		bool initFoliageShader();
		void renderFoliage(Camera* cam);

		GLuint m_ssbo_Visible = 0;
		GLuint m_ssbo_Counter = 0;
		GLuint m_programCull = 0;
		GLuint m_programUpdateCmd = 0;

		bool initCullingShaders();
		void initCullingBuffers();
		void performCulling(const Camera* cam);
		void debugIndirectCmd(GLuint indirectBuf);

		// ==========================================
		// [Phase 5] 史萊姆 (Slime) 相關變數
		// ==========================================
		SimpleMesh m_meshSlime;
		GLuint m_texSlime = 0;
		GLuint m_programSlime = 0;

		// 史萊姆的軌跡物件 (記得在 cpp 裡初始化)
		SCENE::EXPERIMENTAL::Trajectory m_slimeTrajectory;

		// 史萊姆當前位置 (傳給 Shader 用)
		glm::vec3 m_slimePos = glm::vec3(0.0f);
		GLuint m_ssbo_CutMask = 0;  // 新增：記錄每個 instance 是否被史萊姆消除

		// 史萊姆函式
		bool initSlimeResources();
		bool initSlimeShader();
		void renderSlime(Camera* cam, const glm::vec3& pos);
		GLuint loadTexture2D(const std::string& filename);

		// 一個合併後專用的 VAO/VBO/EBO，給 MultiDraw 用
		GLuint m_foliageVAO = 0;
		GLuint m_foliageVBO = 0;
		GLuint m_foliageEBO = 0;

		// 每一種植物在「合併後」大 EBO 里的 index 起始位置 & baseVertex
		GLuint m_firstIndex[3] = { 0,0,0 };
		GLuint m_baseVertex[3] = { 0,0,0 };

		// 建立合併 VAO 的 helper
		bool buildFoliageMultiDrawVAO();

		GLint m_locFoliageView = -1;
		GLint m_locFoliageProj = -1;
		GLint m_locFoliageTex = -1;
	};
}


