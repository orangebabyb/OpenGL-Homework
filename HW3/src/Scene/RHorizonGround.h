#pragma once

#include <glad/glad.h>
#include "../Rendering/Camera/Camera.h"

namespace INANOA {
	namespace SCENE {
		namespace EXPERIMENTAL {
			class HorizonGround
			{
			public:
				explicit HorizonGround(const int numCascade, const Camera* camera);
				virtual ~HorizonGround();

				HorizonGround(const HorizonGround&) = delete;
				HorizonGround(const HorizonGround&&) = delete;
				HorizonGround& operator=(const HorizonGround&) = delete;

			public:
				void init(const Camera* camera);
				void update(const Camera* camera);
				void resize(const Camera* camera);
				void render();

			private:
				const int m_numCascade;
				const int m_numVertex;
				const int m_numIndex;
				const float m_height;
				GLuint m_vertexBufferHandle = 0u;
				float* m_vertexBuffer = nullptr;
				float m_cornerBuffer[12];

				GLuint m_vaoHandle = 0u;

				glm::mat4 m_modelMat;
			};
		}
	}
}



