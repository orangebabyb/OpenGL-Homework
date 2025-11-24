#include "RHorizonGround.h"
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include "../Rendering/ShaderParameterBindingPoint.h"

namespace INANOA {
	namespace SCENE {
		namespace EXPERIMENTAL {
			HorizonGround::HorizonGround(const int numCascade, const Camera* camera) : 
				m_numCascade(numCascade), m_numVertex(numCascade * 4), m_numIndex(numCascade * 6), m_height(0.0f)
			{
				this->init(camera);
			}
			HorizonGround::~HorizonGround() {}


			void HorizonGround::init(const Camera* camera) {
				const int NUM_VERTEX = this->m_numVertex;
				const int NUM_CASCADE = this->m_numCascade;				
				const int NUM_INDEX = this->m_numIndex;

				// initialize the CONSTANT index buffer
				unsigned int* indexBuffer = new unsigned int[NUM_INDEX];
				for (unsigned int i = 0; i < NUM_CASCADE; i++) {
					indexBuffer[i * 6 + 0] = i * 4 + 0;
					indexBuffer[i * 6 + 1] = i * 4 + 1;
					indexBuffer[i * 6 + 2] = i * 4 + 2;

					indexBuffer[i * 6 + 3] = i * 4 + 2;
					indexBuffer[i * 6 + 4] = i * 4 + 3;
					indexBuffer[i * 6 + 5] = i * 4 + 0;
				}

				// create GL_ELEMENT_ARRAY_BUFFER
				GLuint iboHandle = 0u;
				glGenBuffers(1, &iboHandle);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboHandle);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * NUM_INDEX, indexBuffer, GL_STATIC_DRAW);

				delete[] indexBuffer;

				// create a DYNAMIC VBO (only vertex is used)
				this->m_vertexBuffer = new float[NUM_VERTEX * 3]{ 0.0f };
				GLuint vboHandle = 0u;
				glGenBuffers(1, &vboHandle);
				glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
				glBufferData(GL_ARRAY_BUFFER, sizeof(float) * NUM_VERTEX * 3, this->m_vertexBuffer, GL_DYNAMIC_DRAW);
				this->m_vertexBufferHandle = vboHandle;

				// create VAO
				GLuint vaoHandle = 0u;
				glGenVertexArrays(1, &vaoHandle);
				glBindVertexArray(vaoHandle);
				glEnableVertexAttribArray(SHADER_PARAMETER_BINDING::VERTEX_LOCATION);
				glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
				glVertexAttribPointer(SHADER_PARAMETER_BINDING::VERTEX_LOCATION, 3, GL_FLOAT, false, 12, nullptr);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboHandle);		
				glBindVertexArray(0u);
				this->m_vaoHandle = vaoHandle;			
			}

			void HorizonGround::render() {
				// bind vao
				glBindVertexArray(this->m_vaoHandle);
				// submit model matrix
				glUniformMatrix4fv(SHADER_PARAMETER_BINDING::MODEL_MAT_LOCATION, 1, false, glm::value_ptr(this->m_modelMat));				
				// render
				const int indicesPtr = 0;
				glDrawElements(GL_TRIANGLES, this->m_numIndex, GL_UNSIGNED_INT, (GLvoid*)(indicesPtr));
			}

			void HorizonGround::resize(const Camera* camera) {
				const float n = camera->near();
				const float f = camera->far();

				const float depths[] = {
					n,
					n + 0.4f * (f - n),
					f,
				};

				// collect cascade corners
				for (int i = 0; i < this->m_numCascade; i++) {
					float* cascadeVertices = this->m_vertexBuffer + i * 12;
					// get corner in view space
					camera->viewFrustumClipPlaneCornersInViewSpace(depths[i], this->m_cornerBuffer);
					cascadeVertices[5] = -1.0f * this->m_cornerBuffer[2];
					cascadeVertices[8] = -1.0f * this->m_cornerBuffer[11];

					camera->viewFrustumClipPlaneCornersInViewSpace(depths[i + 1], this->m_cornerBuffer);
					cascadeVertices[0] = -1.0f * this->m_cornerBuffer[0];
					cascadeVertices[2] = -1.0f * this->m_cornerBuffer[2];
					cascadeVertices[9] = -1.0f * this->m_cornerBuffer[9];
					cascadeVertices[11] = -1.0f * this->m_cornerBuffer[11];

					cascadeVertices[3] = -1.0f * this->m_cornerBuffer[0];					
					cascadeVertices[6] = -1.0f * this->m_cornerBuffer[9];
				}

				// update buffer
				glBindBuffer(GL_ARRAY_BUFFER, this->m_vertexBufferHandle);
				glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * this->m_numVertex, this->m_vertexBuffer);
			}

			void HorizonGround::update(const Camera* camera) {
				const glm::vec3 viewPos = camera->viewOrig();
				const glm::mat4 viewMat = camera->viewMatrix();

				glm::mat4 tMat = glm::translate(glm::vec3(viewPos.x, this->m_height, viewPos.z));
				glm::mat4 viewT = glm::transpose(viewMat);
				glm::vec3 forward = -1.0f * glm::vec3(viewT[2].x, 0.0, viewT[2].z);
				glm::vec3 y(0.0, 1.0, 0.0);
				glm::vec3 x = glm::normalize(glm::cross(y, forward));

				glm::mat4 rMat;
				rMat[0] = glm::vec4(x, 0.0);
				rMat[1] = glm::vec4(y, 0.0);
				rMat[2] = glm::vec4(forward, 0.0);
				rMat[3] = glm::vec4(0.0, 0.0, 0.0, 1.0);

				this->m_modelMat = tMat * rMat;
			}
		}
	}
}