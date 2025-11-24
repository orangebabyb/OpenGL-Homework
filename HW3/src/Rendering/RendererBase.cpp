#include "RendererBase.h"
#include "ShaderParameterBindingPoint.h"

#include <glm/gtc/type_ptr.hpp>

namespace INANOA {
	namespace OPENGL {
		RendererBase::RendererBase() {
			this->m_viewMat = glm::mat4x4(1.0f);
			this->m_projMat = glm::mat4x4(1.0f);
			this->m_viewPosition = glm::vec4(0.0f);
		}
		RendererBase::~RendererBase() {}

		bool RendererBase::init(const std::string& vsResource, const std::string& fsResource, const int width, const int height) {
			this->m_shaderProgram = ShaderProgram::createShaderProgram(vsResource, fsResource);
			if (this->m_shaderProgram == nullptr) {
				return false;
			}

			this->m_shaderProgram->useProgram();

			// API setting
			glEnable(GL_DEPTH_TEST);
			glLineWidth(2.0f);

			return true;
		}		

		void RendererBase::setCamera(const glm::mat4& projMat, const glm::mat4& viewMat, const glm::vec3& viewOrg) {
			this->m_projMat = projMat;
			this->m_viewMat = viewMat;
			this->m_viewPosition = glm::vec4(viewOrg, 1.0f);

			glUniformMatrix4fv(SHADER_PARAMETER_BINDING::VIEW_MAT_LOCATION, 1, false, glm::value_ptr(this->m_viewMat));
			glUniformMatrix4fv(SHADER_PARAMETER_BINDING::PROJ_MAT_LOCATION, 1, false, glm::value_ptr(this->m_projMat));
		}

		void RendererBase::resize(const int w, const int h) {
			this->m_frameWidth = w;
			this->m_frameHeight = h;
		}

		void RendererBase::clearRenderTarget() {
			static float DEPTH[1] = { 1.0f };
			static float COLOR[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

			glClearBufferfv(GL_COLOR, 0, COLOR);
			glClearBufferfv(GL_DEPTH, 0, DEPTH);
		}
	}	
}
