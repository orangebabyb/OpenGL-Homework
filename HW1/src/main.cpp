#include "Common.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>

// ===================== Todo Start ===========================
using namespace glm;
using namespace std;

struct
{
	int width;
	int height;
} viewportSize_camera;

struct Geometry_obj {
	GLuint vao = 0;
	GLuint vboPos = 0;
	GLuint vboNrm = 0;
	GLuint vboTex = 0;
	GLuint ebo = 0;
	int indexCount = 0;
	GLuint textureID = 0;
};
Geometry_obj Cube, Sphere, Cylinder, Capsule, Plane;

struct Node {
	Geometry_obj* geo = nullptr; // geometry shape
	mat4 local = mat4(1.0f); // matrix
	vec3 color = vec3(1.0f);
	bool  useTexture = false;
	GLuint textureID = 0;
};
Node torso, head, scarf;
Node Reye, Leye;
Node Larm, Rarm, Lhand, Rhand;
Node Lleg, Lfeet, Rleg, Rfeet;
Node hat;

// shader golbal parameter
GLuint program;
GLint uMVLoc;
GLint uProjLoc;
GLint uColorLoc;

// robot position, rotation
vec3 robot_position = vec3(0.0f, 0.0f, 0.0f);
float robot_rotation_y = 0.0f;

//aniomation
bool is_animation_paused = true;
float animation_time = 0.0f;

//texture parameter
GLint uUseTexLoc = -1;
GLint uTexLoc = -1;

// vertex shader
static const char* vertex_shader_source[] =
{
	"#version 410 core\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec3 aNrm;\n"
	"layout(location=2) in vec2 aUV;\n"
	"uniform mat4 mv_matrix;\n"
	"uniform mat4 proj_matrix;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  vUV = aUV;\n"
	"  gl_Position = proj_matrix * mv_matrix * vec4(aPos,1.0);\n"
	"}\n"
};

// fragment shader
static const char* fragment_shader_source[] =
{
	"#version 410 core\n"
	"in vec2 vUV;\n"
	"out vec4 FragColor;\n"
	"uniform vec3  uColor;\n"
	"uniform bool  uUseTex;\n"
	"uniform sampler2D uTex;\n"
	"void main(){\n"
	"  vec4 base = vec4(uColor, 1.0);\n"
	"  if(uUseTex){ base = texture(uTex, vUV); }\n"
	"  FragColor = base;\n"
	"}\n"
};

// Main shader pipeline
void initialize_shader()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, vertex_shader_source, NULL);
	glCompileShader(vs);

	program = glCreateProgram();
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, fragment_shader_source, NULL);
	glCompileShader(fs);

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	printGLShaderLog(vs);
	printGLShaderLog(fs);

	glDeleteShader(vs);
	glDeleteShader(fs);

	uMVLoc = glGetUniformLocation(program, "mv_matrix");
	uProjLoc = glGetUniformLocation(program, "proj_matrix");
	uColorLoc = glGetUniformLocation(program, "uColor");

	uUseTexLoc = glGetUniformLocation(program, "uUseTex");
	uTexLoc = glGetUniformLocation(program, "uTex");
}

static void Load_obj_to_geometry(const vector<MeshData>& meshes, Geometry_obj& out)
{
	const auto& m = meshes[0];
	glGenVertexArrays(1, &out.vao);
	glBindVertexArray(out.vao);

	glGenBuffers(1, &out.vboPos);
	glBindBuffer(GL_ARRAY_BUFFER, out.vboPos);
	glBufferData(GL_ARRAY_BUFFER, m.positions.size() * sizeof(float), m.positions.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); // layout 0: Position
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &out.vboNrm);
	glBindBuffer(GL_ARRAY_BUFFER, out.vboNrm);
	glBufferData(GL_ARRAY_BUFFER, m.normals.size() * sizeof(float), m.normals.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); // layout 1: Normal
	glEnableVertexAttribArray(1);

	// UV
	if (!m.texcoords.empty()) {
		glGenBuffers(1, &out.vboTex);
		glBindBuffer(GL_ARRAY_BUFFER, out.vboTex);
		glBufferData(GL_ARRAY_BUFFER, m.texcoords.size() * sizeof(float), m.texcoords.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0); // layout 2
		glEnableVertexAttribArray(2);
	}
	else {
		std::cerr << "[Warn] OBJ has no texcoords (vt). Texture will not show.\n";
	}

	glGenBuffers(1, &out.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(unsigned int), m.indices.data(), GL_STATIC_DRAW);

	out.indexCount = (int)m.indices.size();

	glBindVertexArray(0);
}

GLuint createTextureFromFile(const char* path, bool genMips = true)
{
	TextureData tex = loadImg(path);
	if (!tex.data) {
		std::cerr << "Failed to load image: " << path << "\n";
		return 0;
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // 也可 CLAMP
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, genMips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// 假設 loadImg() 回傳 RGBA
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data);
	if (genMips) glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	return id;
}

void load_goemetry()
{
	Load_obj_to_geometry(loadObj("assets/Cube.obj"), Cube);
	Load_obj_to_geometry(loadObj("assets/Sphere.obj"), Sphere);
	Load_obj_to_geometry(loadObj("assets/Cylinder.obj"), Cylinder);
	Load_obj_to_geometry(loadObj("assets/Capsule.obj"), Capsule);
	Load_obj_to_geometry(loadObj("assets/Plane.obj"), Plane);
}

void initialize_robot()
{
	float global_scale = 1.0f;

	//torso
	torso.geo = &Cube;
	torso.local = glm::scale(mat4(1.0f), global_scale * vec3(3.0f, 4.0f, 2.0f));
	torso.color = vec3(0.0f, 0.0f, 0.0f);

	//head
	head.geo = &Cube;
	mat4 head_scale = glm::scale(mat4(1.0f), global_scale * vec3(3.0f, 3.0f, 3.0f));
	mat4 head_translate = glm::translate(mat4(1.0f), vec3(0.0f, 3.5 * global_scale, 0.0f));
	head.local = head_translate * head_scale;
	head.color = vec3(1.0f, 1.0f, 1.0f);

	scarf.geo = &Cube;
	mat4 scarf_scale = glm::scale(mat4(1.0f), global_scale * vec3(3.5f, 1.0f, 3.5f));
	mat4 scarf_translate = glm::translate(mat4(1.0f), vec3(0.0f, 4.0 * global_scale, 0.0f));
	scarf.local = scarf_translate * scarf_scale;
	scarf.color = vec3(1.0f, 0.0f, 0.0f);

	//Eyes
	Reye.geo = &Cube;
	mat4 Reye_scale = glm::scale(mat4(1.0f), global_scale * vec3(0.5f, 1.0f, 0.5f));
	mat4 Reye_translate = glm::translate(mat4(1.0f), vec3(-1.0f, 3.0 * global_scale, 2.8f));
	Reye.local = Reye_scale * Reye_translate;
	Reye.color = vec3(0.0f, 0.0f, 0.0f);

	Leye.geo = &Cube;
	mat4 Leye_scale = glm::scale(mat4(1.0f), global_scale * vec3(0.5f, 1.0f, 0.5f));
	mat4 Leye_translate = glm::translate(mat4(1.0f), vec3(1.0f, 3.0 * global_scale, 2.8f));
	Leye.local = Leye_scale * Leye_translate;
	Leye.color = vec3(0.0f, 0.0f, 0.0f);

	//Arm
	Rarm.geo = &Cube;
	mat4 Rarm_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 3.5f, 1.5f));
	mat4 Rarm_translate = glm::translate(mat4(1.0f), vec3(2.25f * global_scale, 0.5f, 0.0f));
	Rarm.local = Rarm_translate * Rarm_scale;
	Rarm.color = vec3(0.1f, 0.1f, 0.1f);

	Larm.geo = &Cube;
	mat4 Larm_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 3.5f, 1.5f));
	mat4 Larm_translate = glm::translate(mat4(1.0f), vec3(-2.25f * global_scale, 0.5f, 0.0f));
	Larm.local = Larm_translate * Larm_scale;
	Larm.color = vec3(0.1f, 0.1f, 0.1f);

	//Hand
	Rhand.geo = &Cube;
	mat4 Rhand_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 1.0f, 1.5f));
	mat4 Rhand_translate = glm::translate(mat4(1.0f), vec3(0.0f, -2.0f, 0.0f));
	Rhand.local = Rarm_translate * Rhand_translate * Rhand_scale;
	Rhand.color = vec3(1.0f, 0.5f, 0.5f);

	Lhand.geo = &Cube;
	mat4 Lhand_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 1.0f, 1.5f));
	mat4 Lhand_translate = glm::translate(mat4(1.0f), vec3(0.0f, -2.0f, 0.0f));
	Lhand.local = Larm_translate * Lhand_translate * Lhand_scale;
	Lhand.color = vec3(1.0f, 0.5f, 0.5f);

	//Leg
	Rleg.geo = &Cube;
	mat4 Rleg_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 3.5f, 1.5f));
	mat4 Rleg_translate = glm::translate(mat4(1.0f), vec3(0.75f, -3.75f, 0.0f));
	Rleg.local = Rleg_translate * Rleg_scale;
	Rleg.color = vec3(0.1f, 0.1f, 0.1f);

	Lleg.geo = &Cube;
	mat4 LLeg_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 3.5f, 1.5f));
	mat4 LLeg_translate = glm::translate(mat4(1.0f), vec3(-0.75f, -3.75f, 0.0f));
	Lleg.local = LLeg_translate * LLeg_scale;
	Lleg.color = vec3(0.1f, 0.1f, 0.1f);

	//Foot
	Rfeet.geo = &Cube;
	mat4 Rfeet_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 1.0f, 1.5f));
	mat4 Rfeet_translate = glm::translate(mat4(1.0f), vec3(0.0f, -2.0f, 0.0f));
	Rfeet.local = Rleg_translate * Rfeet_translate * Rfeet_scale;
	Rfeet.color = vec3(0.5f, 0.5f, 0.1f);

	Lfeet.geo = &Cube;
	mat4 Lfeet_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.5f, 1.0f, 1.5f));
	mat4 Lfeet_translate = glm::translate(mat4(1.0f), vec3(0.0f, -2.0f, 0.0f));
	Lfeet.local = LLeg_translate * Lfeet_translate * Lfeet_scale;
	Lfeet.color = vec3(0.5f, 0.5f, 0.1f);

	hat.geo = &Cube;
	mat4 hat_scale = glm::scale(mat4(1.0f), global_scale * vec3(1.0f, 1.0f, 1.0f));
	mat4 hat_translate = glm::translate(mat4(1.0f), vec3(0.0f, 5.5f * global_scale, 0.0f));
	hat.local = hat_translate * hat_scale;
	hat.color = vec3(1.0f);      
	hat.useTexture = true;      
	hat.textureID = createTextureFromFile("assets/brick.png");
}

void Reshape(GLFWwindow* window, int width, int height)
{
	viewportSize_camera.width = width;
	viewportSize_camera.height = height;
	float viewportAspect = (float)width / (float)height;
	//matrices.eye.proj = perspective(deg2rad(70.0f), viewportAspect, 0.1f, 1000.0f);
	//matrices.eye.view = lookAt(vec3(0.0f, 0.0f, 40.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
}

void drawObject(Node* node, const mat4& modelMatrix, const mat4& viewMatrix, const mat4& projMatrix)
{
	if (node->geo != nullptr)
	{
		mat4 mv = viewMatrix * modelMatrix;
		glUniformMatrix4fv(uMVLoc, 1, GL_FALSE, value_ptr(mv));
		glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, value_ptr(projMatrix));
		glUniform3fv(uColorLoc, 1, value_ptr(node->color));

		if (node->useTexture && node->textureID) {
			glUniform1i(uUseTexLoc, GL_TRUE);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, node->textureID);
			glUniform1i(uTexLoc, 0);
		}
		else {
			glUniform1i(uUseTexLoc, GL_FALSE);
		}

		glBindVertexArray(node->geo->vao);
		glDrawElements(GL_TRIANGLES, node->geo->indexCount, GL_UNSIGNED_INT, 0);
	}
}

// keyboard control
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		float move_speed = 0.5f;
		float rotate_speed = radians(5.0f);

		switch (key)
		{
		case GLFW_KEY_W:
			robot_position.y += move_speed;
			break;
		case GLFW_KEY_S:
			robot_position.y -= move_speed;
			break;
		case GLFW_KEY_A:
			robot_position.x -= move_speed;
			break;
		case GLFW_KEY_D:
			robot_position.x += move_speed; 
			break;
		case GLFW_KEY_SPACE:
			robot_rotation_y += rotate_speed;
			break;
		}
	}
}

// mouse control
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	robot_position.z += (float)yoffset * 0.5f;
}

glm::mat4 makeSwingRotationAroundPoint(
	glm::vec3 pivot, 
	glm::vec3 axis,
	float time, 
	float swings_per_sec = 0.2f,
	float max_deg = 80.0f
)
{
	// omega = 2 * pi * freq
	float omega = 2.0f * glm::pi<float>() * swings_per_sec;

	// -max_deg ~ +max_deg
	float angle = glm::radians(max_deg) * sinf(omega * time);

	// transition matrix
	return glm::translate(glm::mat4(1.0f), pivot) *
		glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis)) *
		glm::translate(glm::mat4(1.0f), -pivot);
}
// ===================== Todo End =============================

vec3 cubeColor = glm::vec3(1.0f, 0.3f, 0.3f);
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
void on_display()
{
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
	//glClear(GL_COLOR_BUFFER_BIT);

	// ===================== Todo Start =============================
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);

	mat4 view = lookAt(vec3(0, 0, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 proj = perspective(radians(50.0f), 1280.0f / 800.0f, 1.0f, 1000.0f);
	mat4 rootMatrix = translate(mat4(1.0f), robot_position) 
		            * rotate(mat4(1.0f), robot_rotation_y, vec3(0, 1, 0));

	// drawObject
	drawObject(&torso, rootMatrix * torso.local, view, proj);

	drawObject(&head, rootMatrix * head.local, view, proj);

	drawObject(&scarf, rootMatrix * scarf.local, view, proj);

	drawObject(&Reye, rootMatrix * Reye.local, view, proj);

	drawObject(&Leye, rootMatrix * Leye.local, view, proj);

	drawObject(&hat, rootMatrix * hat.local, view, proj);

	// animation part
	vec3  P = vec3(0, 1.75f, 0);
	vec3  AXIS = vec3(1, 0, 0);
	float angle = is_animation_paused ? animation_time : (float)glfwGetTime() * radians(90.0f);
	if (!is_animation_paused) animation_time = angle;
	//float angle = (float)glfwGetTime() * radians(90.0f);

	// ====== Rarm ======
	mat4 Rarm_translate = translate(mat4(1.0f), vec3(2.25f, 0.5f, 0.0f));
	mat4 Rarm_scale = scale(mat4(1.0f), vec3(1.5f, 3.5f, 1.5f));
	mat4 Rarm_Rotation = makeSwingRotationAroundPoint(P, AXIS, angle);
	mat4 Rarm_basis = rootMatrix * Rarm_translate * Rarm_Rotation;
	mat4 Rarm_geom = Rarm_basis * Rarm_scale;
	drawObject(&Rarm, Rarm_geom, view, proj);

	// ====== Rhand ======
	mat4 Rhand_relative = translate(mat4(1.0f), vec3(0.0f, -2.25f, 0.0f)) *
		scale(mat4(1.0f), vec3(1.5f, 1.0f, 1.5f));
	mat4 Rhand_basis = Rarm_basis * Rhand_relative;
	drawObject(&Rhand, Rhand_basis, view, proj);

	// ====== Larm ======
	mat4 Larm_translate = translate(mat4(1.0f), vec3(-2.25f, 0.5f, 0.0f));
	mat4 Larm_scale = scale(mat4(1.0f), vec3(1.5f, 3.5f, 1.5f));
	mat4 Larm_Rotation = makeSwingRotationAroundPoint(P, AXIS, -angle);
	mat4 Larm_basis = rootMatrix * Larm_translate * Larm_Rotation;
	mat4 Larm_geom = Larm_basis * Larm_scale;
	drawObject(&Larm, Larm_geom, view, proj);

	// ====== Lhand ======
	mat4 Lhand_relative = translate(mat4(1.0f), vec3(0.0f, -2.25f, 0.0f)) *
		scale(mat4(1.0f), vec3(1.5f, 1.0f, 1.5f));
	mat4 Lhand_basis = Larm_basis * Lhand_relative;
	drawObject(&Lhand, Lhand_basis, view, proj);

	// ====== Rleg ======
	mat4 Rleg_translate = translate(mat4(1.0f), vec3(0.75f, -3.75f, 0.0f));
	mat4 Rleg_scale = scale(mat4(1.0f), vec3(1.5f, 3.5f, 1.5f));
	mat4 Rleg_Rotation = makeSwingRotationAroundPoint(P, AXIS, -angle);
	mat4 Rleg_basis = rootMatrix * Rleg_translate * Rleg_Rotation;
	mat4 Rleg_geom = Rleg_basis * Rleg_scale;
	drawObject(&Rleg, Rleg_geom, view, proj);

	// ====== Rfeet ======
	mat4 Rfeet_relative = translate(mat4(1.0f), vec3(0.0f, -2.25f, 0.0f)) *
		scale(mat4(1.0f), vec3(1.5f, 1.0f, 1.5f));
	mat4 Rfeet_basis = Rleg_basis * Rfeet_relative;
	drawObject(&Rfeet, Rfeet_basis, view, proj);

	// ====== Lleg ======
	mat4 Lleg_translate = translate(mat4(1.0f), vec3(-0.75f, -3.75f, 0.0f));
	mat4 Lleg_scale = scale(mat4(1.0f), vec3(1.5f, 3.5f, 1.5f));
	mat4 Lleg_Rotation = makeSwingRotationAroundPoint(P, AXIS, angle);
	mat4 Lleg_basis = rootMatrix * Lleg_translate * Lleg_Rotation;
	mat4 Lleg_geom = Lleg_basis * Lleg_scale;
	drawObject(&Lleg, Lleg_geom, view, proj);

	// ====== Lfeet ======
	mat4 Lfeet_relative = translate(mat4(1.0f), vec3(0.0f, -2.25f, 0.0f)) *
		scale(mat4(1.0f), vec3(1.5f, 1.0f, 1.5f));
	mat4 Lfeet_basis = Lleg_basis * Lfeet_relative;
	drawObject(&Lfeet, Lfeet_basis, view, proj);

	// ===================== Todo End ===============================
}

void on_gui()
{
	// Reference: https://raw.githubusercontent.com/ocornut/imgui/refs/heads/master/examples/example_glfw_opengl3/main.cpp
	// Our state
	static bool show_demo_window = true;
	static bool show_another_window = false;

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

		// ===================== Todo Start =============================
		ImGui::Separator();
		if (ImGui::Button(is_animation_paused ? "Start Animation" : "Pause Animation"))
		{
			is_animation_paused = !is_animation_paused;
		}
		// ===================== Todo End ===============================
		ImGui::End();
	}

	// 3. Show another simple window.
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
	if (window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD\n";
		return -1;
	}
	glfwSwapInterval(1); // Enable vsync
	//glfwSetKeyCallback(window, key_callback);
	//glfwSetScrollCallback(window, scroll_callback);

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

	// ===================== Todo Start =============================
	initialize_shader();
	load_goemetry();
	initialize_robot();
	glfwSetKeyCallback(window, key_callback);
	glfwSetScrollCallback(window, scroll_callback);
	// ===================== Todo End ===============================

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