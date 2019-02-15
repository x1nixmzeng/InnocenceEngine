#pragma once
#include "../common/InnoType.h"
#include "../common/InnoClassTemplate.h"

#include "../component/MeshDataComponent.h"
#include "../component/TextureDataComponent.h"
#include "../component/GLMeshDataComponent.h"
#include "../component/GLTextureDataComponent.h"
#include "../component/GLFrameBufferComponent.h"
#include "../component/GLShaderProgramComponent.h"
#include "../component/GLRenderPassComponent.h"

INNO_PRIVATE_SCOPE GLRenderingSystemNS
{
	GLRenderPassComponent* addGLRenderPassComponent(unsigned int RTNum, GLFrameBufferDesc glFrameBufferDesc, TextureDataDesc RTDesc);
	void addRenderTargetTextures(GLRenderPassComponent* GLRPC, TextureDataDesc RTDesc, unsigned int colorAttachmentIndex);
	void setDrawBuffers(unsigned int RTNum);
	bool resizeGLRenderPassComponent(GLRenderPassComponent* GLRPC, GLFrameBufferDesc glFrameBufferDesc);

	GLMeshDataComponent* generateGLMeshDataComponent(MeshDataComponent* rhs);
	GLTextureDataComponent* generateGLTextureDataComponent(TextureDataComponent* rhs);

	bool initializeGLShaderProgramComponent(GLShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths);
	bool initializeGLMeshDataComponent(GLMeshDataComponent * rhs, const std::vector<Vertex>& vertices, const std::vector<Index>& indices);
	bool initializeGLTextureDataComponent(GLTextureDataComponent * rhs, TextureDataDesc textureDataDesc, const std::vector<void*>& textureData);
	GLTextureDataDesc getGLTextureDataDesc(const TextureDataDesc& textureDataDesc);

	GLShaderProgramComponent* addGLShaderProgramComponent(const EntityID& rhs);

	bool deleteShaderProgram(GLShaderProgramComponent* rhs);

	GLMeshDataComponent* addGLMeshDataComponent(const EntityID& rhs);
	GLTextureDataComponent* addGLTextureDataComponent(const EntityID& rhs);

	GLMeshDataComponent* getGLMeshDataComponent(const EntityID& rhs);
	GLTextureDataComponent* getGLTextureDataComponent(const EntityID& rhs);

	GLuint getUniformLocation(GLuint shaderProgram, const std::string& uniformName);
	GLuint getUniformBlockIndex(GLuint shaderProgram, const std::string& uniformBlockName);
	GLuint generateUBO(GLuint UBOSize);
	void bindUniformBlock(GLuint UBO, GLuint UBOSize, GLuint program, const std::string & uniformBlockName, GLuint uniformBlockBindingPoint);
	void updateTextureUniformLocations(GLuint program, const std::vector<std::string>& UniformNames);

	void updateUBOImpl(const GLint& UBO, size_t size, const void* UBOValue);

	template<typename T>
	void updateUBO(const GLint& UBO, const T& UBOValue)
	{
		updateUBOImpl(UBO, sizeof(T), &UBOValue);
	}

	void updateUniform(const GLint uniformLocation, bool uniformValue);
	void updateUniform(const GLint uniformLocation, int uniformValue);
	void updateUniform(const GLint uniformLocation, float uniformValue);
	void updateUniform(const GLint uniformLocation, float x, float y);
	void updateUniform(const GLint uniformLocation, float x, float y, float z);
	void updateUniform(const GLint uniformLocation, float x, float y, float z, float w);
	void updateUniform(const GLint uniformLocation, const mat4& mat);

	void attachDepthRT(TextureDataComponent * TDC, GLTextureDataComponent * GLTDC, GLFrameBufferComponent * GLFBC);
	void attachCubemapDepthRT(TextureDataComponent * TDC, GLTextureDataComponent * GLTDC, GLFrameBufferComponent * GLFBC, unsigned int textureIndex, unsigned int mipLevel);
	void attachColorRT(TextureDataComponent * TDC, GLTextureDataComponent * GLTDC, GLFrameBufferComponent * GLFBC, unsigned int colorAttachmentIndex);
	void attachCubemapColorRT(TextureDataComponent * TDC, GLTextureDataComponent * GLTDC, GLFrameBufferComponent * GLFBC, unsigned int colorAttachmentIndex, unsigned int textureIndex, unsigned int mipLevel);
	void activateShaderProgram(GLShaderProgramComponent* GLShaderProgramComponent);

	void drawMesh(const EntityID& rhs);
	void drawMesh(MeshDataComponent* MDC);
	void drawMesh(size_t indicesSize, MeshPrimitiveTopology MeshPrimitiveTopology, GLMeshDataComponent* GLMDC);
	void activateTexture(TextureDataComponent* TDC, int activateIndex);
	void activateTexture(GLTextureDataComponent* GLTDC, int activateIndex);

	void bindFBC(GLFrameBufferComponent* val);
	void cleanFBC(GLFrameBufferComponent* val);
	void copyDepthBuffer(GLFrameBufferComponent* src, GLFrameBufferComponent* dest);
	void copyStencilBuffer(GLFrameBufferComponent* src, GLFrameBufferComponent* dest);
	void copyColorBuffer(GLFrameBufferComponent* src, GLFrameBufferComponent* dest);
}
