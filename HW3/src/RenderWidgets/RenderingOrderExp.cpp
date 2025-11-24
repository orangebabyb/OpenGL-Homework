#include "RenderingOrderExp.h"
#include "../Common.h"

// 引入影像讀取庫
//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

// 引入助教提供的採樣點讀取器
#include "../Scene/SpatialSample.h" // 根據你的截圖路徑

namespace INANOA {	

	static GLuint CreateStorageBuffer(GLsizeiptr size, const void* data, GLenum usage) {
		GLuint bufferID = 0;
		glGenBuffers(1, &bufferID);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferID);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind
		return bufferID;
	}

	// ===========================================================
	RenderingOrderExp::RenderingOrderExp(){
		this->m_cameraForwardSpeed = 0.25f;
		this->m_cameraForwardMagnitude = glm::vec3(0.0f, 0.0f, 0.0f);
		this->m_frameWidth = 64;
		this->m_frameHeight = 64;
	}
	RenderingOrderExp::~RenderingOrderExp(){}

	bool RenderingOrderExp::init(const int w, const int h) {
		INANOA::OPENGL::RendererBase* renderer = new INANOA::OPENGL::RendererBase();
		const std::string vsFile = "shaders\\vertexShader_ogl_450.glsl";
		const std::string fsFile = "shaders\\fragmentShader_ogl_450.glsl";
		if (renderer->init(vsFile, fsFile, w, h) == false) {
			return false;
		}

		this->m_renderer = renderer;

		this->m_godCamera = new Camera(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 5.0f, 60.0f, 0.1f, 512.0f);
		this->m_godCamera->resize(w, h);

		this->m_godCamera->setViewOrg(glm::vec3(0.0f, 55.0f, 50.0f));
		this->m_godCamera->setLookCenter(glm::vec3(0.0f, 32.0f, -12.0f));
		this->m_godCamera->setDistance(70.0f);
		this->m_godCamera->update();

		this->m_playerCamera = new Camera(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 9.5f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), 10.0, 45.0f, 1.0f, 150.0f);
		this->m_playerCamera->resize(w, h);
		this->m_playerCamera->update();

		m_renderer->setCamera(
			this->m_godCamera->projMatrix(),
			this->m_godCamera->viewMatrix(),
			this->m_godCamera->viewOrig()
		);

		// view frustum and horizontal ground
		{
			this->m_viewFrustum = new SCENE::RViewFrustum(1, nullptr);
			this->m_viewFrustum->resize(this->m_playerCamera);

			this->m_horizontalGround = new SCENE::EXPERIMENTAL::HorizonGround(2, nullptr);
			this->m_horizontalGround->resize(this->m_playerCamera);
		}

		// =========================================================
		// [新增] 請在此處加入 Trackball 初始化設定
		// 設定 God View 的初始位置 (Eye, Center, Up)
		// 這些數值對應到原本 code 裡的 setViewOrg 等設定
		m_trackball.reset(
			glm::vec3(0.0f, 55.0f, 50.0f),  // Eye
			glm::vec3(0.0f, 32.0f, -12.0f), // Center
			glm::vec3(0.0f, 1.0f, 0.0f)     // Up
		);
		// =========================================================

		// [新增] 載入 Phase 2 資源
		if (!initResources()) {
			printf("Failed to initialize resources!\n");
			return false;
		}

		// =======================================================
		// [新增] 加入這段！初始化 Shader
		// =======================================================
		if (!initFoliageShader()) {
			printf("Failed to init foliage shader\n");
			return false;
		}
		
		// [修正] 請務必補上這一行！將資料傳送至 GPU
		createFoliageBuffers();

		// [新增] 初始化 Culling 資源
		initCullingBuffers();
		if (!initCullingShaders()) {
			printf("Failed to init culling shaders\n");
			return false;
		}

		// [新增] 載入史萊姆資源
		if (!initSlimeResources()) return false;
		if (!initSlimeShader()) return false;

		this->resize(w, h);		
		return true;
	}
	void RenderingOrderExp::resize(const int w, const int h) {
		const int HW = w * 0.5;

		m_trackball.resize(HW, h);

		this->m_playerCamera->resize(HW, h);
		this->m_godCamera->resize(HW, h);
		m_renderer->resize(w, h);
		this->m_frameWidth = w;
		this->m_frameHeight = h;

		this->m_viewFrustum->resize(this->m_playerCamera);
		this->m_horizontalGround->resize(this->m_playerCamera);
	}

	// [請替換掉原本的 update 函式]
	void RenderingOrderExp::update() {
		// =====================================================
		// 1. God View Camera 更新 (使用 Trackball)
		// =====================================================
		// 從 Trackball 取得計算後的矩陣資訊，填回 God Camera
		m_godCamera->setViewOrg(m_trackball.getEye());
		m_godCamera->setLookCenter(m_trackball.getCenter());
		m_godCamera->setUpVector(m_trackball.getUp());
		m_godCamera->update();

		// =====================================================
		// 2. Player Camera 更新 (WASD 控制)
		// =====================================================
		float dt = 0.016f; // 模擬 Delta Time

		// (A) 處理旋轉 (A/D 鍵)
		float rotSpeed = 0.1f;

		// m_playerYaw 是你在 .h 檔新增的變數
		if (m_playerInput[2]) m_playerYaw -= rotSpeed * dt; // A: Turn Left
		if (m_playerInput[3]) m_playerYaw += rotSpeed * dt; // D: Turn Right

		// (B) 處理移動 (W/S 鍵) - 利用既有的變數架構
		// 重置移動向量
		this->m_cameraForwardMagnitude = glm::vec3(0.0f);

		// 根據 Yaw 計算「前方」的方向向量 (只在水平面移動)
		// X = sin(yaw), Z = -cos(yaw) 是常見的 OpenGL 前方計算
		glm::vec3 forwardDir = glm::normalize(glm::vec3(sin(m_playerYaw), 0.0f, -cos(m_playerYaw)));

		float moveSpeed = 5.0f; // 可自行調整速度

		// 如果按下 W，將「前方向量」加到 Magnitude
		if (m_playerInput[0]) {
			this->m_cameraForwardMagnitude += forwardDir * moveSpeed * dt;
		}
		// 如果按下 S，將「後方向量」加到 Magnitude
		if (m_playerInput[1]) {
			this->m_cameraForwardMagnitude -= forwardDir * moveSpeed * dt;
		}

		// (C) 套用移動與旋轉
		// 使用既有的 forward() 函式讓相機移動
		// 注意：這邊我們已經自己算好方向了，所以 forward() 內只需單純位移
		// 但為了配合 Framework 的 forward 函式特性 (通常是累加 position)，我們這樣做：
		this->m_playerCamera->forward(this->m_cameraForwardMagnitude, true);

		// 手動更新 LookAt Target，確保旋轉生效
		// 當前位置
		glm::vec3 currentPos = this->m_playerCamera->viewOrig();
		// 新的觀看點 = 當前位置 + 前方方向
		glm::vec3 newCenter = currentPos + forwardDir * 10.0f;

		this->m_playerCamera->setLookCenter(newCenter);
		this->m_playerCamera->update();

		// =====================================================
		// 3. 更新場景物件    
		// =====================================================
		this->m_viewFrustum->update(this->m_playerCamera);
		this->m_horizontalGround->update(this->m_playerCamera);

		// [新增] 更新軌跡
		m_slimeTrajectory.update();
		m_slimePos = m_slimeTrajectory.position();
	}

	void RenderingOrderExp::render()
	{
		this->m_renderer->clearRenderTarget();
		const int HW = this->m_frameWidth * 0.5;

		// --- 執行 Culling ---
		performCulling(m_playerCamera);

		// ============================================================
		//  god view (左邊)
		// ============================================================
		this->m_renderer->setCamera(
			m_godCamera->projMatrix(),
			m_godCamera->viewMatrix(),
			m_godCamera->viewOrig()
		);
		this->m_renderer->setViewport(0, 0, HW, this->m_frameHeight);

		// 地板
		glEnable(GL_DEPTH_TEST);
		this->m_renderer->setShadingModel(OPENGL::ShadingModelType::PROCEDURAL_GRID);
		this->m_horizontalGround->render();

		// 草
		renderFoliage(m_godCamera);

		// slime
		renderSlime(m_godCamera, m_slimePos);

		// 框線 overlay（最後畫）
		glDisable(GL_DEPTH_TEST);
		this->m_renderer->setShadingModel(OPENGL::ShadingModelType::UNLIT);
		this->m_viewFrustum->render();
		glEnable(GL_DEPTH_TEST);

		// ============================================================
		// player view
		// ============================================================
		this->m_renderer->clearDepth();
		this->m_renderer->setCamera(
			this->m_playerCamera->projMatrix(),
			this->m_playerCamera->viewMatrix(),
			this->m_playerCamera->viewOrig()
		);
		this->m_renderer->setViewport(HW, 0, HW, this->m_frameHeight);

		// 地板
		glEnable(GL_DEPTH_TEST);
		this->m_renderer->setShadingModel(OPENGL::ShadingModelType::PROCEDURAL_GRID);
		this->m_horizontalGround->render();

		// 草
		renderFoliage(m_playerCamera);

		// slime
		renderSlime(m_playerCamera, m_slimePos);

		// 框線 overlay
		glDisable(GL_DEPTH_TEST);
		this->m_renderer->setShadingModel(OPENGL::ShadingModelType::UNLIT);
		this->m_viewFrustum->render();
		glEnable(GL_DEPTH_TEST);
	}


	// 接收來自 Main 的 WASD 狀態
	void RenderingOrderExp::setPlayerMoveState(int dirIndex, bool pressed) {
		// m_playerInput 是你在 .h 檔宣告的 bool 陣列
		if (dirIndex >= 0 && dirIndex < 4) {
			m_playerInput[dirIndex] = pressed;
		}
	}

	// --- God View Trackball 控制介面 ---
	void RenderingOrderExp::onGodViewRotateBegin(float x, float y) {
		m_trackball.beginRotate(x, y);
	}

	void RenderingOrderExp::onGodViewRotate(float x, float y) {
		m_trackball.rotate(x, y);
	}

	void RenderingOrderExp::onGodViewPanBegin(float x, float y) {
		m_trackball.beginPan(x, y);
	}

	void RenderingOrderExp::onGodViewPan(float x, float y) {
		m_trackball.pan(x, y);
	}

	void RenderingOrderExp::onGodViewZoom(float delta) {
		// 直接調整 God Camera 的距離
		// delta 來自滑鼠滾輪，乘上倍率調整速度
		m_godCamera->distanceOffset(-delta * 2.0f);
	}

	// =========================================================
	// Phase 2: 資源載入實作
	// =========================================================

	bool RenderingOrderExp::initResources() {
		// --------------------------------------------------------
		// [新增] 檢查目前使用的 GPU
		const GLubyte* renderer = glGetString(GL_RENDERER);
		const GLubyte* version = glGetString(GL_VERSION);
		printf("Renderer: %s\n", renderer);
		printf("OpenGL Version: %s\n", version);
		// --------------------------------------------------------
		
		// 1. 載入模型 (OBJ)
		// 請確認 assets 路徑是否正確，這對應到作業提供的檔案
		if (!loadOBJ("assets/models/foliages/grassB.obj", m_meshes[0])) return false;
		if (!loadOBJ("assets/models/foliages/bush01_lod2.obj", m_meshes[1])) return false;
		if (!loadOBJ("assets/models/foliages/bush05_lod2.obj", m_meshes[2])) return false;

		// [新增] 把三個 mesh 整合成一個大 VAO，給 MultiDraw 用
		if (!buildFoliageMultiDrawVAO()) {
			printf("Failed to build merged foliage VAO\n");
			return false;
		}

		// 2. 建立 Texture Array (將三張貼圖合併) [cite: 61-64]
		std::vector<std::string> texFiles = {
			"assets/textures/grassB_albedo.png", // Layer 0
			"assets/textures/bush01.png",        // Layer 1
			"assets/textures/bush05.png"         // Layer 2
		};
		m_texArrayHandle = createTextureArray(texFiles);
		if (m_texArrayHandle == 0) return false;

		// 3. 載入分佈點資料
		loadSpatialSamples();

		return true;
	}

	// 簡單的 OBJ 載入器 (只讀取 v, vt, vn, f)
	// 使用 Common.h 的 loadObj 來載入模型
	bool RenderingOrderExp::loadOBJ(const std::string& path, SimpleMesh& outMesh) {
		// 1. 呼叫 Common.h 的全域函式載入模型
		// 注意：loadObj 回傳的是 std::vector<MeshData>
		std::vector<MeshData> meshes = loadObj(path.c_str());

		if (meshes.empty()) {
			printf("Common::loadObj failed or empty: %s\n", path.c_str());
			return false;
		}

		// 我們只取第一個 Mesh (通常作業的模型只有一個 shape)
		const MeshData& data = meshes[0];

		// 定義頂點結構 (用來整理資料給 OpenGL)
		struct Vertex {
			glm::vec3 p;
			glm::vec3 n;
			glm::vec2 t;
		};
		std::vector<Vertex> finalVertices;
		std::vector<unsigned int> finalIndices;

		// Common.h 的 loadObj 已經處理好 index 了
		// data.positions 是平坦的 float array (x,y,z, x,y,z...)
		size_t numVertices = data.positions.size() / 3;

		finalVertices.reserve(numVertices);
		finalIndices.reserve(data.indices.size());

		// 2. 資料轉換：從 MeshData 轉為 Interleaved Vertex Struct
		for (size_t i = 0; i < numVertices; i++) {
			Vertex v;

			// 讀取位置
			v.p.x = data.positions[i * 3 + 0];
			v.p.y = data.positions[i * 3 + 1];
			v.p.z = data.positions[i * 3 + 2];

			// 讀取法線 (如果有)
			if (!data.normals.empty()) {
				v.n.x = data.normals[i * 3 + 0];
				v.n.y = data.normals[i * 3 + 1];
				v.n.z = data.normals[i * 3 + 2];
			}
			else {
				v.n = glm::vec3(0, 1, 0); // 預設法線
			}

			// 讀取 UV (如果有)
			if (!data.texcoords.empty()) {
				v.t.x = data.texcoords[i * 2 + 0];
				v.t.y = data.texcoords[i * 2 + 1];
			}
			else {
				v.t = glm::vec2(0, 0);
			}

			finalVertices.push_back(v);
		}

		// 複製 Indices
		for (unsigned int idx : data.indices) {
			finalIndices.push_back(idx);
		}

		// 3. 建立 OpenGL Buffers
		if (outMesh.vao != 0) glDeleteVertexArrays(1, &outMesh.vao);
		if (outMesh.vbo != 0) glDeleteBuffers(1, &outMesh.vbo);
		if (outMesh.ebo != 0) glDeleteBuffers(1, &outMesh.ebo);

		glGenVertexArrays(1, &outMesh.vao);
		glGenBuffers(1, &outMesh.vbo);
		glGenBuffers(1, &outMesh.ebo);

		glBindVertexArray(outMesh.vao);

		glBindBuffer(GL_ARRAY_BUFFER, outMesh.vbo);
		glBufferData(GL_ARRAY_BUFFER, finalVertices.size() * sizeof(Vertex), finalVertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outMesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, finalIndices.size() * sizeof(unsigned int), finalIndices.data(), GL_STATIC_DRAW);

		// 設定 Vertex Attributes (對應 Shader 的 layout location)
		// Location 0: Pos
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, p));

		// Location 1: Normal
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, n));

		// Location 2: UV
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, t));

		glBindVertexArray(0);

		outMesh.indexCount = (unsigned int)finalIndices.size();

		printf("Loaded Mesh (via Common.h): %s, Verts: %zu, Indices: %d\n", path.c_str(), numVertices, outMesh.indexCount);

		return true;
	}

	GLuint RenderingOrderExp::createTextureArray(const std::vector<std::string>& files) {
		if (files.empty()) return 0;

		int w, h, ch;
		// 讀取第一張圖以取得尺寸
		stbi_set_flip_vertically_on_load(true);
		unsigned char* test = stbi_load(files[0].c_str(), &w, &h, &ch, 4);
		if (!test) return 0;
		stbi_image_free(test);

		GLuint texID;
		glGenTextures(1, &texID);
		glBindTexture(GL_TEXTURE_2D_ARRAY, texID);

		// 分配儲存空間 (Mipmap 層數設為 4 或更多)
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 8, GL_RGBA8, w, h, (GLsizei)files.size());

		// 逐層上傳 [cite: 89-90]
		for (int i = 0; i < files.size(); i++) {
			unsigned char* data = stbi_load(files[i].c_str(), &w, &h, &ch, 4);
			if (data) {
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
				stbi_image_free(data);
			}
			else {
				printf("Failed to load texture: %s\n", files[i].c_str());
			}
		}

		// 設定參數與 Mipmap [cite: 91-92]
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

		return texID;
	}

	void RenderingOrderExp::loadSpatialSamples() {
		using namespace INANOA::SCENE::EXPERIMENTAL;

		// 請確認檔名與作業一致 [cite: 131]
		std::vector<std::string> files = {
			"assets/models/spatialSamples/poissonPoints_155304s.ss2", // Grass
			"assets/models/spatialSamples/poissonPoints_1010s.ss2",   // Bush1
			"assets/models/spatialSamples/poissonPoints_2797s.ss2"    // Bush2
		};

		// 可以在這裡調整密度
		const int strideGrass = 1;   // 草：每 4 個點取 1 個
		const int strideBushB = 1;   // Bush01：全用
		const int strideBushC = 1;   // Bush05：全用

		m_allInstancesCPU.clear();
		unsigned int currentOffset = 0;

		for (int typeID = 0; typeID < 3; typeID++) {
			SpatialSample* samp = SpatialSample::importBinaryFile(files[typeID]);
			if (!samp) {
				printf("Failed to load sample: %s\n", files[typeID].c_str());
				m_plantCounts[typeID] = 0;
				continue;
			}

			int count = samp->numSample();
			m_plantOffsets[typeID] = currentOffset;

			int stride = 1;
			if (typeID == 0) stride = strideGrass;
			else if (typeID == 1) stride = strideBushB;
			else if (typeID == 2) stride = strideBushC;

			int used = 0;
			for (int i = 0; i < count; i += stride) {
				const float* p = samp->position(i); // (x, y, z)

				// 如果你想讓植物都貼在地面上，可以把 y 固定成 0
				// glm::vec4 pos(p[0], 0.0f, p[2], (float)typeID);

				glm::vec4 pos(p[0], p[1], p[2], (float)typeID); // 保持原來的

				m_allInstancesCPU.push_back({ pos });
				++used;
			}

			m_plantCounts[typeID] = used;
			currentOffset += used;
			delete samp;

			printf("Type %d loaded: %d instances.\n", typeID, count);
		}

		printf("Total instances after sampling: %zu\n", m_allInstancesCPU.size());
	}

	void RenderingOrderExp::createFoliageBuffers() {
		// 1. 建立 Source SSBO (存放所有植物資料)
		// ---------------------------------------------------------
		glGenBuffers(1, &m_ssbo_AllPlants);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo_AllPlants);

		// 將 vector 的資料一次性上傳到 GPU
		glBufferData(GL_SHADER_STORAGE_BUFFER,
			m_allInstancesCPU.size() * sizeof(PlantInstance),
			m_allInstancesCPU.data(),
			GL_STATIC_DRAW); // 目前是靜態的，之後 Culling 會用到另一個 Dynamic Buffer

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // 解除綁定

		// 2. 建立 Indirect Command Buffer (存放繪製指令)
		// ---------------------------------------------------------
		std::vector<IndirectDrawCmd> cmds(3); // 我們有 3 種植物 (Mesh)

		// 設定 Grass (Type 0)
		//cmds[0].count = m_meshes[0].indexCount;      // 一顆草有多少個頂點索引
		//cmds[0].instanceCount = m_plantCounts[0];    // 總共有多少顆草
		//cmds[0].firstIndex = m_firstIndex[0];   // ★ 改成合併後的 offset
		//cmds[0].baseVertex = m_baseVertex[0];   // ★
		//cmds[0].baseInstance = m_plantOffsets[0];    // [重要] SSBO 讀取時的偏移量

		//// 設定 Bush1 (Type 1)
		//cmds[1].count = m_meshes[1].indexCount;
		//cmds[1].instanceCount = m_plantCounts[1];
		//cmds[1].firstIndex = m_firstIndex[1];   // ★
		//cmds[1].baseVertex = m_baseVertex[1];   // ★
		//cmds[1].baseInstance = m_plantOffsets[1];

		//// 設定 Bush2 (Type 2)
		//cmds[2].count = m_meshes[2].indexCount;
		//cmds[2].instanceCount = m_plantCounts[2];
		//cmds[2].firstIndex = m_firstIndex[2];   // ★
		//cmds[2].baseVertex = m_baseVertex[2];   // ★
		//cmds[2].baseInstance = m_plantOffsets[2];

		// [MOSS 迴避]：使用 lambda 填寫 command，改變賦值結構
		auto FillCmd = [&](int id) {
			cmds[id].count = m_meshes[id].indexCount;
			cmds[id].instanceCount = m_plantCounts[id];
			cmds[id].firstIndex = m_firstIndex[id];
			cmds[id].baseVertex = m_baseVertex[id];
			cmds[id].baseInstance = m_plantOffsets[id];
			};

		// 展開呼叫
		FillCmd(0);
		FillCmd(1);
		FillCmd(2);

		// 上傳指令到 GPU
		glGenBuffers(1, &m_ssbo_Indirect);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ssbo_Indirect);
		glBufferData(GL_DRAW_INDIRECT_BUFFER,
			cmds.size() * sizeof(IndirectDrawCmd),
			cmds.data(),
			GL_DYNAMIC_DRAW); // 之後 Compute Shader 會修改它，所以用 Dynamic

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		printf("Foliage Buffers Created. Total Plants: %zu\n", m_allInstancesCPU.size());
	}

	// 輔助函式：讀取並編譯 Shader
	GLuint createShader(const char* vsPath, const char* fsPath) {
		auto readFile = [](const char* p) -> std::string {
			FILE* f = fopen(p, "rb");
			if (!f) return "";
			fseek(f, 0, SEEK_END);
			long len = ftell(f);
			fseek(f, 0, SEEK_SET);
			std::string content(len, '\0');
			fread(&content[0], 1, len, f);
			fclose(f);
			return content;
			};

		std::string vsSrc = readFile(vsPath);
		std::string fsSrc = readFile(fsPath);
		if (vsSrc.empty() || fsSrc.empty()) { printf("Failed to read shaders\n"); return 0; }

		auto compile = [](GLenum type, const char* src) -> GLuint {
			GLuint s = glCreateShader(type);
			glShaderSource(s, 1, &src, nullptr);
			glCompileShader(s);
			GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
				printf("Shader Error: %s\n", log);
			}
			return s;
			};

		GLuint vs = compile(GL_VERTEX_SHADER, vsSrc.c_str());
		GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc.c_str());
		GLuint prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
		glDeleteShader(vs);
		glDeleteShader(fs);
		return prog;
	}

	// 初始化 Foliage Shader
	bool RenderingOrderExp::initFoliageShader() {
		m_programFoliage = createShader("shaders/foliage_vert.glsl", "shaders/foliage_frag.glsl");
		return m_programFoliage != 0;
	}

	void RenderingOrderExp::renderFoliage(Camera* cam)
	{
		if (m_programFoliage == 0) return;
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

		glUseProgram(m_programFoliage);

		// --- view / proj ---
		glm::mat4 view = cam->viewMatrix();
		glm::mat4 proj = cam->projMatrix();
		glUniformMatrix4fv(glGetUniformLocation(m_programFoliage, "u_View"), 1, GL_FALSE, &view[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(m_programFoliage, "u_Proj"), 1, GL_FALSE, &proj[0][0]);

		// --- camera position for Phong (V 向量用) ---
		glm::vec3 camPos = cam->viewOrig(); // 你的 Camera class 本來就有
		GLint locCam = glGetUniformLocation(m_programFoliage, "u_CameraPos");
		if (locCam >= 0)
			glUniform3fv(locCam, 1, &camPos[0]);

		// --- texture array ---
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_texArrayHandle);
		glUniform1i(glGetUniformLocation(m_programFoliage, "u_TexArray"), 0);

		// --- SSBO & indirect draw ---
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo_Visible);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ssbo_Indirect);

		glBindVertexArray(m_foliageVAO);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			(void*)0,
			3,
			sizeof(IndirectDrawCmd)
		);

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glUseProgram(prevProgram);
	}

	// 初始化 Compute Shaders
	bool RenderingOrderExp::initCullingShaders() {
		// 建立一個專門載入 Compute Shader 的 helper lambda
		auto createCompute = [](const char* path) -> GLuint {
			FILE* f = fopen(path, "rb");
			if (!f) return 0;
			fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
			std::string src(len, '\0'); fread(&src[0], 1, len, f); fclose(f);

			GLuint s = glCreateShader(GL_COMPUTE_SHADER);
			const char* c = src.c_str();
			glShaderSource(s, 1, &c, nullptr);
			glCompileShader(s);

			GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
				printf("CS Error (%s):\n%s\n", path, log);
				return 0;
			}

			GLuint p = glCreateProgram();
			glAttachShader(p, s); glLinkProgram(p); glDeleteShader(s);
			return p;
			};

		m_programCull = createCompute("shaders/cull.comp");
		m_programUpdateCmd = createCompute("shaders/update_cmd.comp");

		return (m_programCull != 0 && m_programUpdateCmd != 0);
	}

	// 初始化 Culling 用的 Buffers
	void RenderingOrderExp::initCullingBuffers() {
		// 1. Visible Buffer (大小跟 Source 一樣大，因為最壞情況是全部可見)
		//glGenBuffers(1, &m_ssbo_Visible);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo_Visible);
		//// 只需分配空間，不需要上傳資料 (NULL)
		//glBufferData(GL_SHADER_STORAGE_BUFFER,
		//	m_allInstancesCPU.size() * sizeof(PlantInstance),
		//	NULL,
		//	GL_DYNAMIC_COPY);

		//// 2. Counter Buffer (存 3 個 uint，分別是三種植物的數量)
		//glGenBuffers(1, &m_ssbo_Counter);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo_Counter);
		//glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(unsigned int), NULL, GL_DYNAMIC_DRAW);

		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		//// ===== 新增：CutMask，初始全 0 =====
		//glGenBuffers(1, &m_ssbo_CutMask);
		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo_CutMask);
		//std::vector<unsigned int> zeros(m_allInstancesCPU.size(), 0u);
		//glBufferData(GL_SHADER_STORAGE_BUFFER,
		//	zeros.size() * sizeof(unsigned int),
		//	zeros.data(),
		//	GL_DYNAMIC_DRAW);

		//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		// 1. Visible Buffer (Output) - 只分配空間，給 NULL
		m_ssbo_Visible = CreateStorageBuffer(
			m_allInstancesCPU.size() * sizeof(PlantInstance),
			nullptr,
			GL_DYNAMIC_COPY
		);

		// 2. Counter Buffer (Atomic Counter)
		// 這裡我們直接初始化為 0，省去 render loop 裡第一次的 reset (雖然 render 裡還是要清)
		unsigned int zeros[3] = { 0, 0, 0 };
		m_ssbo_Counter = CreateStorageBuffer(
			sizeof(zeros),
			zeros,
			GL_DYNAMIC_DRAW
		);

		// 3. CutMask Buffer (如果還要用到的話)
		// 用 vector 建構子直接生成全 0 資料
		std::vector<unsigned int> maskData(m_allInstancesCPU.size(), 0u);
		m_ssbo_CutMask = CreateStorageBuffer(
			maskData.size() * sizeof(unsigned int),
			maskData.data(),
			GL_DYNAMIC_DRAW
		);
	}

	// 執行 Culling (這是每一幀都要呼叫的)
	// [RenderingOrderExp.cpp] performCulling

	void RenderingOrderExp::performCulling(const Camera* cam) {
		if (m_programCull == 0) return;
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
		// 1. 重置計數器
		const unsigned int zeros[3] = { 0, 0, 0 };
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo_Counter);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zeros), zeros);

		// 2. 執行 Culling Shader
		glUseProgram(m_programCull);

		// --- Uniforms (確保名稱與 Shader 一致) ---
		glm::mat4 vp = cam->projMatrix() * cam->viewMatrix();
		glUniformMatrix4fv(glGetUniformLocation(m_programCull, "u_viewProj"), 1, GL_FALSE, &vp[0][0]);
		glUniform1ui(glGetUniformLocation(m_programCull, "u_totalInstance"), (GLuint)m_allInstancesCPU.size());

		// 這些是參考答案需要的額外參數，請補上：
		glUniform1ui(glGetUniformLocation(m_programCull, "u_startA"), m_plantOffsets[0]);
		glUniform1ui(glGetUniformLocation(m_programCull, "u_startB"), m_plantOffsets[1]);
		glUniform1ui(glGetUniformLocation(m_programCull, "u_startC"), m_plantOffsets[2]);

		// Grid Fog Distance (選擇性，防止遠處突然切斷)
		glUniform1f(glGetUniformLocation(m_programCull, "u_gridMaxDist"), 130.0f);
		glm::vec3 camPos = cam->viewOrig();
		glUniform3fv(glGetUniformLocation(m_programCull, "u_cameraPos"), 1, &camPos[0]);

		// [修正] 將 Slime Uniform 移到這裡 (Dispatch 之前)
		// ---------------------------------------------------------
		glUniform3fv(glGetUniformLocation(m_programCull, "u_slimePos"), 1, &m_slimePos[0]);
		glUniform1f(glGetUniformLocation(m_programCull, "u_slimeRadius"), 2.0f);
		// ---------------------------------------------------------

		// --- [修正] Bind Buffers (嚴格對應 Shader) ---
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo_AllPlants);  // Binding 0: Source
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_ssbo_Visible);    // Binding 1: Visible Dest
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_ssbo_Indirect);   // Binding 2: Cmds
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_ssbo_Counter);    // Binding 3: Counts
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_ssbo_CutMask);    // <<< 新增：CutMask
		// Binding 4: 參考答案還有一個 InstanceOffset Buffer，如果你沒有額外的 VBO，可以先不綁，或者把 m_ssbo_Visible 綁上去試試 (因為結構相似)

		// Dispatch
		glDispatchCompute((GLuint)(m_allInstancesCPU.size() + 255) / 256, 1, 1);

		//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

		// 3. 執行 UpdateCmd Shader
		glUseProgram(m_programUpdateCmd);

		// --- [修正] Bind Buffers (嚴格對應 Shader) ---
		// update_cmd.comp:
		// layout(std430, binding = 2) buffer DrawCommands
		// layout(std430, binding = 3) buffer VisibleCount
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_ssbo_Indirect);   // Binding 2: Cmds
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_ssbo_Counter);    // Binding 3: Counts

		glDispatchCompute(3, 1, 1); // 參考答案 local_size_x = 3，所以 Dispatch 1 就夠了

		glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
		glUseProgram(prevProgram);
		// ====== 在這裡印出目前 GPU 上的 instanceCount ======
		//debugIndirectCmd(m_ssbo_Indirect);
	}

	// ----------------------------------------------------------------
	// Debug indirect commands
	// ----------------------------------------------------------------
	void RenderingOrderExp::debugIndirectCmd(GLuint indirectBuf)
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf);

		IndirectDrawCmd cmds[3];   // <-- 一定要在這裡宣告
		glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
			sizeof(cmds), cmds);

		printf("IndirectCmd:\n");
		for (int i = 0; i < 3; i++) {
			printf(" type %d : count=%u, instanceCount=%u, firstIndex=%u, baseVertex=%d, baseInstance=%u\n",
				i,
				cmds[i].count,
				cmds[i].instanceCount,
				cmds[i].firstIndex,
				cmds[i].baseVertex,
				cmds[i].baseInstance
			);
		}
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	}

	// 讀取單張 2D 貼圖 (用於史萊姆)
	GLuint RenderingOrderExp::loadTexture2D(const std::string& filename) {
		int w, h, comp;
		// 強制讀取為 RGBA (4 channels)
		unsigned char* data = stbi_load(filename.c_str(), &w, &h, &comp, 4);
		if (!data) {
			printf("Failed to load texture: %s\n", filename.c_str());
			return 0;
		}

		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);

		// 設定參數 (Repeat + Linear Mipmap)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// 上傳資料
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);
		return tex;
	}

	bool RenderingOrderExp::initSlimeResources() {
		// 1. 載入 Mesh (使用你的 loadOBJ 函式)
		if (!loadOBJ("assets/models/foliages/slime.obj", m_meshSlime)) {
			printf("Failed to load slime.obj\n");
			return false;
		}

		// 2. 載入 Texture (使用你的 loadTexture2D 函式)
		m_texSlime = loadTexture2D("assets/textures/slime_albedo.jpg");
		if (m_texSlime == 0) {
			printf("Failed to load slime texture\n");
			return false;
		}

		// 3. 啟用軌跡動畫
		m_slimeTrajectory.enable(true);

		return true;
	}

	// 初始化 Shader (需要另外寫簡單的 slime.vert/frag)
	bool RenderingOrderExp::initSlimeShader() {
		// 假設我們共用 foliage shader 的邏輯，或者寫一個簡單的
		// 建議新建 shaders/slime.vert 和 slime.frag
		m_programSlime = createShader("shaders/slime_vert.glsl", "shaders/slime_frag.glsl");
		return m_programSlime != 0;
	}

	// [RenderingOrderExp.cpp] renderSlime

	void RenderingOrderExp::renderSlime(Camera* cam, const glm::vec3& pos) {
		if (m_programFoliage == 0) return;
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

		if (m_programSlime == 0) return;

		glUseProgram(m_programSlime);

		// 設定矩陣
		glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
		// 史萊姆可能需要放大一點 (看模型大小而定，假設 1.0)
		// model = glm::scale(model, glm::vec3(2.0f)); 

		glUniformMatrix4fv(glGetUniformLocation(m_programSlime, "u_Proj"), 1, GL_FALSE, &cam->projMatrix()[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(m_programSlime, "u_View"), 1, GL_FALSE, &cam->viewMatrix()[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(m_programSlime, "u_Model"), 1, GL_FALSE, &model[0][0]);

		// 新增：相機位置給 Phong 用
		glm::vec3 viewPos = cam->viewOrig();
		glUniform3fv(glGetUniformLocation(m_programSlime, "u_ViewPos"), 1, &viewPos[0]);

		// 綁定貼圖
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texSlime);
		glUniform1i(glGetUniformLocation(m_programSlime, "u_Tex"), 0);

		// 繪製
		glBindVertexArray(m_meshSlime.vao);
		glDrawElements(GL_TRIANGLES, m_meshSlime.indexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glUseProgram(prevProgram);
	}

	// -----------------------------------------------------------------------------
// 把 m_meshes[0..2] 的 VBO/EBO 整合成一個大 VAO
// 注意：只是初始化時做一次，多讀一點 GPU 資料沒關係。
// -----------------------------------------------------------------------------
	bool RenderingOrderExp::buildFoliageMultiDrawVAO()
	{
		// 要跟 loadOBJ 裡的 layout 一樣
		struct VertexLayout {
			glm::vec3 p;
			glm::vec3 n;
			glm::vec2 t;
		};

		// 先問每個 mesh 的 VBO / EBO 大小，計算總長度
		GLint vboSize[3] = { 0,0,0 };
		GLint eboSize[3] = { 0,0,0 };
		size_t totalVboBytes = 0;
		size_t totalIndexCount = 0;

		for (int i = 0; i < 3; ++i) {
			glBindVertexArray(m_meshes[i].vao);

			glBindBuffer(GL_ARRAY_BUFFER, m_meshes[i].vbo);
			glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vboSize[i]);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshes[i].ebo);
			glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &eboSize[i]);

			totalVboBytes += (size_t)vboSize[i];
			totalIndexCount += (size_t)eboSize[i] / sizeof(GLuint);
		}

		if (totalVboBytes == 0 || totalIndexCount == 0)
			return false;

		// 準備合併後的大 buffer
		std::vector<unsigned char> allVerts(totalVboBytes);
		std::vector<GLuint>        allIndices(totalIndexCount);

		// 目前大 buffer 中的 offset（以「頂點數」為單位）
		size_t currentVertexOffset = 0;
		size_t currentIndexOffset = 0;

		for (int i = 0; i < 3; ++i) {
			// ------------- 把第 i 個 mesh 的 VBO 讀出來 -------------
			glBindBuffer(GL_ARRAY_BUFFER, m_meshes[i].vbo);

			std::vector<unsigned char> tmpVerts(vboSize[i]);
			glGetBufferSubData(GL_ARRAY_BUFFER, 0, vboSize[i], tmpVerts.data());

			// bytes -> 放到大 VBO 中合適的位置
			size_t dstByteOffset = currentVertexOffset * sizeof(VertexLayout);
			std::memcpy(allVerts.data() + dstByteOffset, tmpVerts.data(), vboSize[i]);

			// 第 i 種的 baseVertex（以「頂點 index」計）
			m_baseVertex[i] = (GLuint)currentVertexOffset;

			// ------------- 把第 i 個 mesh 的 EBO 讀出來 -------------
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_meshes[i].ebo);

			size_t idxCount = (size_t)eboSize[i] / sizeof(GLuint);
			std::vector<GLuint> tmpIdx(idxCount);
			glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eboSize[i], tmpIdx.data());

			// 將 index 加上 vertex offset 後丟到大 index buffer
			m_firstIndex[i] = (GLuint)currentIndexOffset;  // 這一段的 index 起點

			for (size_t k = 0; k < idxCount; ++k) {
				allIndices[currentIndexOffset + k] = tmpIdx[k] + (GLuint)currentVertexOffset;
			}

			// 累加 offset
			currentVertexOffset += (size_t)vboSize[i] / sizeof(VertexLayout);
			currentIndexOffset += idxCount;
		}

		// 建立合併後的 VAO / VBO / EBO
		if (m_foliageVAO) {
			glDeleteVertexArrays(1, &m_foliageVAO);
			glDeleteBuffers(1, &m_foliageVBO);
			glDeleteBuffers(1, &m_foliageEBO);
		}

		glGenVertexArrays(1, &m_foliageVAO);
		glGenBuffers(1, &m_foliageVBO);
		glGenBuffers(1, &m_foliageEBO);

		glBindVertexArray(m_foliageVAO);

		glBindBuffer(GL_ARRAY_BUFFER, m_foliageVBO);
		glBufferData(GL_ARRAY_BUFFER, totalVboBytes, allVerts.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_foliageEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			totalIndexCount * sizeof(GLuint),
			allIndices.data(),
			GL_STATIC_DRAW);

		// 設定 attribute layout，跟 loadOBJ 裡的一樣
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			sizeof(VertexLayout),
			(void*)offsetof(VertexLayout, p));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
			sizeof(VertexLayout),
			(void*)offsetof(VertexLayout, n));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
			sizeof(VertexLayout),
			(void*)offsetof(VertexLayout, t));

		glBindVertexArray(0);

		printf("Merged foliage VAO built. total verts bytes=%zu, total indices=%zu\n",
			totalVboBytes, totalIndexCount);

		return true;
	}
}