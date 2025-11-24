#pragma once

#include <glad/glad.h>
#include <glm/mat4x4.hpp>

#include "Shader.h"
#include "ShaderParameterBindingPoint.h"

namespace INANOA {
	namespace OPENGL {
		enum class ShadingModelType : int {
			PROCEDURAL_GRID = 0,
			UNLIT = 5
		};

		class RendererBase
		{
		public:
			explicit RendererBase();
			virtual ~RendererBase();

			RendererBase(const RendererBase&) = delete;
			RendererBase(const RendererBase&&) = delete;
			RendererBase& operator=(const RendererBase&) = delete;

		public:
			bool init(const std::string& vsResource, const std::string& fsResource, const int width, const int height);
			void resize(const int w, const int h);
			
			void setCamera(const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg);

			void clearRenderTarget();

		public:
			inline void setShadingModel(const ShadingModelType type) {
				glUniform1i(SHADER_PARAMETER_BINDING::SHADING_MODEL_ID_LOCATION, static_cast<int>(type));
			}
			inline void setViewport(const int x, const int y, const int w, const int h) {
				glViewport(x, y, w, h);
			}
			inline void clearDepth(const double d = 1.0) {
				glClearDepth(d);
			}

		private:
			glm::mat4 m_viewMat;
			glm::mat4 m_projMat;
			glm::vec4 m_viewPosition;

			int m_frameWidth = 64;
			int m_frameHeight = 64;

			ShaderProgram* m_shaderProgram = nullptr;
		};
	}
	
}



