#pragma once

#include <string>
#include <glad/glad.h>

namespace INANOA {
	namespace OPENGL {
		enum class ShaderStatus {
			READY,
			NULL_SHADER, NULL_SHADER_CODE
		};

		enum class ShaderProgramStatus {
			READY,
			PROGRAM_ID_READY,
			NULL_VERTEX_SHADER, NULL_FRAGMENT_SHADER,
			NULL_VERTEX_SHADER_FRAGMENT_SHADER
		};


		class Shader
		{
		public:
			explicit Shader(const GLenum shaderType);
			virtual ~Shader();

			// prohibit copy constructor
			Shader(const Shader&) = delete;
			// prohibit assignment
			Shader& operator=(const Shader) = delete;

		public:
			bool createShaderFromFile(const std::string& fileFullpath);
			void appendShaderCode(const std::string& code);
			bool compileShader();
			void releaseShader();

		public:
			inline std::string shaderInfoLog() const { return this->m_shaderInfoLog; }
			inline ShaderStatus status() const { return this->m_shaderStatus; }
			inline GLuint shaderId() const { return this->m_shaderId; }
			inline GLenum shaderType() const { return this->m_shaderType; }

		private:
			const GLenum m_shaderType;
			GLuint m_shaderId;
			std::string m_shaderInfoLog;
			std::string m_shaderCode;
			ShaderStatus m_shaderStatus;
		};


		class ShaderProgram
		{
		public:
			explicit ShaderProgram();
			virtual ~ShaderProgram();

			// prohibit copy constructor
			ShaderProgram(const ShaderProgram&) = delete;
			// prohibit assignment
			ShaderProgram& operator=(const ShaderProgram&) = delete;

		public:
			bool init();
			bool attachShader(const Shader* shader);
			ShaderProgramStatus checkStatus();
			void linkProgram();
			
		public:
			inline void useProgram(){ glUseProgram(this->m_programId); }

		public:
			inline GLuint programId() const { return this->m_programId; }
			inline ShaderProgramStatus status() const { return this->m_shaderProgramStatus; }

		private:
			GLuint m_programId;
			bool m_vsReady = false;
			bool m_fsReady = false;
			bool m_csReady = false;

			ShaderProgramStatus m_shaderProgramStatus;

		public:
			static ShaderProgram* createShaderProgram(const std::string& vsResource, const std::string& fsResource);
			static ShaderProgram* createShaderProgramForComputeShader(const std::string& csResource);
		};
	}
	


}

