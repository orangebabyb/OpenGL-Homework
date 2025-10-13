#pragma once

// include OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// include tinyobjloader for Wavefront OBJ format import
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// include glm for vector/matrix math
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// include stb image for texture image import
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// include platform-dependent library for chdir()
#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#endif

// include standard libraries
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <algorithm>

// define __FILENAME__ for OpenGL window title, and __FILEPATH__ for chdir()
#ifdef _MSC_VER
	#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
    #define __FILEPATH__ ((std::string(__FILE__).substr(0, std::string(__FILE__).rfind('\\'))).c_str())
#else
	#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
    #define __FILEPATH__ ((std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/'))).c_str())
#endif

// define deg2rad for converting degree to radian
#define deg2rad(x) ((x)*((3.1415926f)/(180.0f)))

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
	_TextureData(void) :
		width(0),
		height(0),
		data(0)
	{
	}

	int width;
	int height;
	unsigned char* data;
} TextureData;

// forward declaration of functions
void printGLContextInfo(void);
void printGLShaderLog(const GLuint shader);
void printGLError(void);
TextureData loadPNG(const char* const imgFilePath);

// print OpenGL context related information
void printGLContextInfo(void)
{
	printf("GL_VENDOR: %s\n", glGetString (GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString (GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString (GL_VERSION));
	printf("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));
}

// print the compile log of an OpenGL shader object, if GL_COMPILE_STATUS is GL_FALSE
void printGLShaderLog(const GLuint shader)
{
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if(isCompiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// maxLength already includes the NULL terminator. no need to +1
		GLchar* errorLog = new GLchar[maxLength];
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

		printf("%s\n", errorLog);
		delete[] errorLog;
	}
}

// get the OpenGL error code and print its text representation
// note that the error code is reset to GL_NO_ERROR after the glGetError() call
void printGLError(void)
{
    GLenum code = glGetError();
    switch(code)
    {
    case GL_NO_ERROR:
        std::cout << "GL_NO_ERROR" << std::endl;
        break;
    case GL_INVALID_ENUM:
        std::cout << "GL_INVALID_ENUM" << std::endl;
        break;
    case GL_INVALID_VALUE:
        std::cout << "GL_INVALID_VALUE" << std::endl;
        break;
    case GL_INVALID_OPERATION:
        std::cout << "GL_INVALID_OPERATION" << std::endl;
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        std::cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << std::endl;
        break;
    case GL_OUT_OF_MEMORY:
        std::cout << "GL_OUT_OF_MEMORY" << std::endl;
        break;
    case GL_STACK_UNDERFLOW:
        std::cout << "GL_STACK_UNDERFLOW" << std::endl;
        break;
    case GL_STACK_OVERFLOW:
        std::cout << "GL_STACK_OVERFLOW" << std::endl;
        break;
    default:
        std::cout << "GL_ERROR (" << std::hex << code << std::dec << ")" << std::endl;
    }
}

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadImg(const char* const imgFilePath)
{
	TextureData texture;
	int components;

	// load the texture with stb image, force RGBA (4 components required)
	stbi_uc *data = stbi_load(imgFilePath, &texture.width, &texture.height, &components, 4);

	// is the image successfully loaded?
	if(data != NULL)
	{
		// copy the raw data
		size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
		texture.data = new unsigned char[dataSize];
		memcpy(texture.data, data, dataSize);

		// mirror the image vertically to comply with OpenGL convention
		for (size_t i = 0; i < texture.width; ++i)
		{
			for (size_t j = 0; j < texture.height / 2; ++j)
			{
				for(size_t k = 0; k < 4; ++k)
				{
					size_t coord1 = (j * texture.width + i) * 4 + k;
					size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
					std::swap(texture.data[coord1], texture.data[coord2]);
				}
			}
		}

		// release the loaded image
		stbi_image_free(data);
	}

    return texture;
}

// Change loadobj format from tinyobj V1.X to V0.9.X 
// in order not to change OLD Leture Program too mush Zzz...
// You can write your own loadObj by yourself.
typedef struct _MeshData 
{
	// if OBJ preserves vertex order, you can use element array buffer for memory efficiency
	// If no data return empty vector
	std::vector<float> positions;
	std::vector<float> normals;
	std::vector<float> texcoords;
	std::vector<unsigned int> indices;
	std::vector<unsigned char> num_vertices;
	std::vector<int> material_ids; // per-face material ID
} MeshData;

// load obj file
std::vector<MeshData> loadObj(const char* const objFilePath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objFilePath);
	printf("Load Models ! Shapes size %d Material size %d\n", shapes.size(), materials.size());
	if (!warn.empty()) {
		std::cout << warn << std::endl;
	}
	if (!err.empty()) {
		std::cout << err << std::endl;
	}
	if (!ret) {
		exit(1);
	}

	std::vector<MeshData> meshes;
	
	for (int s = 0; s < shapes.size(); ++s) {
		MeshData mesh;

		int index_offset = 0;
		for (int f = 0; f < shapes[s].mesh.num_face_vertices.size(); ++f) {
			int fv = shapes[s].mesh.num_face_vertices[f];
			for (int v = 0; v < fv; ++v) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				if (idx.vertex_index != -1) {
					mesh.positions.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
					mesh.positions.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
					mesh.positions.push_back(attrib.vertices[3 * idx.vertex_index + 2]);
				}
				if (idx.texcoord_index != -1) {
					mesh.texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
					mesh.texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
				}
				if (idx.normal_index != -1) {
					mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 0]);
					mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 1]);
					mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
				}
				mesh.indices.push_back(index_offset + v);
			}
			index_offset += fv;
		}

		meshes.push_back(mesh);
	}

	return meshes;
}
