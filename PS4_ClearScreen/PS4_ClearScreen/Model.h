#pragma once



#include <gnmx/context.h>

struct Vertex
{
	float x, y, z;
	float r, g, b;
	float u, v;
};

extern Vertex s_quadVerts[4];
extern uint16_t s_quadInds[6];

enum eVertexElements
{
	kVertexPos = 0,
	kVertexCol,
	kVertexUV,

	kVertexCount
};


class Model
{
public:
	Model();
	~Model();

	int Init();

	void Render();

private:
	Vertex* m_pVertData = nullptr;
	uint16_t* m_pIndData = nullptr;

	sce::Gnm::Buffer m_vertexBuffers[eVertexElements::kVertexCount];

	sce::Gnmx::VsShader* m_pVsShader = nullptr;
	sce::Gnmx::PsShader* m_pPsShader = nullptr;

	void* m_fsMem;

	float z = 0.0f;

	uint32_t m_shaderModifier;

	sce::Gnmx::InputOffsetsCache m_vsInputOffsetCache, m_psInputOffsetCache;

};

