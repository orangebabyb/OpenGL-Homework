#pragma once

#include "../Rendering/Camera/Camera.h"

#include <glad/glad.h>

namespace INANOA {
	namespace SCENE {
		class RViewFrustum
		{
		public:
			explicit RViewFrustum(const int numCascade, const Camera* camera);
			virtual ~RViewFrustum();

			RViewFrustum(const RViewFrustum&) = delete;
			RViewFrustum(const RViewFrustum&&) = delete;
			RViewFrustum& operator=(const RViewFrustum&) = delete;

		public:
			void init(const Camera* camera);
			inline void update(const Camera* camera) {
				this->m_modelMat = camera->modelMat();
			}
			void resize(const Camera* camera);
			void render();


		private:
			const int m_numCascade;
			const int m_numVertex;
			const int m_numIndex;
			GLuint m_vertexBufferHandle = 0u;
			float* m_vertexBuffer = nullptr;

			GLuint m_vaoHandle = 0u;

			glm::mat4 m_modelMat;
		};
	}
}



