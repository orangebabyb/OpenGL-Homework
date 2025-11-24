#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "RenderWidgets/RenderingOrderExp.h"

// =================================================================
// [新增] 強制使用高效能顯卡 (NVIDIA & AMD)
// 把這段加在全域範圍 (Global Scope)，不要放在函式裡
// =================================================================
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
// =================================================================

INANOA::RenderingOrderExp* renderer = nullptr;
const int INIT_WIDTH = 1344;
const int INIT_HEIGHT = 756;
double PROGRAM_FPS = 0.0;
double FRAME_MS = 0.0;

// ==========================================
// [新增] 請補上這三個變數宣告
// ==========================================
bool isLeftMousePressed = false;
bool isMiddleMousePressed = false;
bool isRightMousePressed = false;

bool on_init(int displayWidth, int displayHeight)
{
	// Initialize render
	renderer = new INANOA::RenderingOrderExp();
	if (renderer->init(displayWidth, displayHeight) == false)
	{
		return false;
	}
	return true;
}

void on_resize(GLFWwindow* window, int w, int h)
{
	renderer->resize(w, h);
}

inline void on_display()
{
	renderer->update();
	renderer->render();
}

inline void on_gui()
{
	// Show statistics window
	{
		static char fpsBuf[] = "fps: 000000000.000000000";
		static char msBuf[] = "ms: 000000000.000000000";

		sprintf_s(fpsBuf + 5, 16, "%.5f", PROGRAM_FPS);
		sprintf_s(msBuf + 4, 16, "%.5f", (1000.0 / PROGRAM_FPS));

		ImGui::Begin("Information");
		ImGui::Text(fpsBuf);
		ImGui::Text(msBuf);
		ImGui::End();
	}
}

void on_destroy()
{
	delete renderer;
	renderer = nullptr;
}

void on_mouse_button(GLFWwindow* window, int button, int action, int mods) {
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	bool pressed = (action == GLFW_PRESS);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		isLeftMousePressed = pressed;
		if (pressed) renderer->onGodViewRotateBegin((float)x, (float)y);
	}
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) { // 假設中鍵平移
		isMiddleMousePressed = pressed;
		if (pressed) renderer->onGodViewPanBegin((float)x, (float)y);
	}
}

void on_cursor_pos(GLFWwindow* window, double x, double y)
{
	// 讓 ImGui 優先取得輸入，避免操作 UI 時轉動相機
	if (ImGui::GetIO().WantCaptureMouse) return;

	if (isLeftMousePressed) renderer->onGodViewRotate((float)x, (float)y);
	if (isMiddleMousePressed) renderer->onGodViewPan((float)x, (float)y);
}

void on_key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// 定義按鍵狀態：按下=true, 放開=false
	bool isPressed = (action != GLFW_RELEASE);

	if (key == GLFW_KEY_W) renderer->setPlayerMoveState(0, isPressed); // 0: Forward
	if (key == GLFW_KEY_S) renderer->setPlayerMoveState(1, isPressed); // 1: Backward
	if (key == GLFW_KEY_A) renderer->setPlayerMoveState(2, isPressed); // 2: Left
	if (key == GLFW_KEY_D) renderer->setPlayerMoveState(3, isPressed); // 3: Right
}

void on_scroll(GLFWwindow* window, double xoffset, double yoffset)
{
	if (ImGui::GetIO().WantCaptureMouse) return;
	renderer->onGodViewZoom((float)yoffset);
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

	const char* glsl_version = "#version 460";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);


	// Create window with graphics context
	float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	const int windowWidth = (int)(INIT_WIDTH * main_scale);
	const int windowHeight = (int)(INIT_HEIGHT * main_scale);
	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "AS3_Template", nullptr, nullptr);
	if (window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD\n";
		return -1;
	}
	glfwSwapInterval(0); // Disable vsync

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

	// Register callbacks (before ImGui_ImplGlfw_InitForOpenGL)
	glfwSetKeyCallback(window, on_key);
	glfwSetScrollCallback(window, on_scroll);
	glfwSetMouseButtonCallback(window, on_mouse_button);
	glfwSetCursorPosCallback(window, on_cursor_pos);
	glfwSetFramebufferSizeCallback(window, on_resize);

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Init program
	if (on_init(windowWidth, windowHeight) == false)
	{
		glfwTerminate();
		return 0;
	}

	// FPS calculation
	double previousTimeStamp = glfwGetTime();
	int frameCounter = 0;

	// Main loop
	while (!glfwWindowShouldClose(window))
	{
		// FPS calculation
		const double timeStamp = glfwGetTime();
		const double deltaTime = timeStamp - previousTimeStamp;
		if (deltaTime >= 1.0) {
			PROGRAM_FPS = frameCounter / deltaTime;

			// reset
			frameCounter = 0;
			previousTimeStamp = timeStamp;
		}
		frameCounter = frameCounter + 1;

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
	on_destroy();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}