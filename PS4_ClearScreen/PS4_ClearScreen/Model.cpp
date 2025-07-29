#include "Model.h"
#include "common/shader_loader.h"
#include "Context.h"

#include "std_cbuffer.h"

//Vertex s_quadVerts[4] =
//{
//	// position				// colour			// UV
// {-0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    0.0f, 1.0f},
// { 0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    1.0f, 1.0f},
// {-0.5f,  0.5f, 0.0f,    0.7f, 1.0f, 1.0f,    0.0f, 0.0f},
// { 0.5f,  0.5f, 0.0f,    1.0f, 0.7f, 1.0f,    1.0f, 0.0f},
//};

Vertex s_quadVerts[4] =
{
	// position				// colour			// UV
 {-0.5f, -0.5f, 0.0f,    1.0, 0.0f, 0.0f,    0.0f, 1.0f},
 { 0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f,    1.0f, 1.0f},
 {-0.5f,  0.5f, 0.0f,    0.0f, 0.0f, 1.0f,    0.0f, 0.0f},
 { 0.5f,  0.5f, 0.0f,    0.0f, 0.0f, 0.0f,    1.0f, 0.0f},
};

uint16_t s_quadInds[6] =
{
 0, 1, 2,
 1, 3, 2
};

Model::Model()
{
}

Model::~Model()
{
}

int Model::Init()
{
	Context* context = Context::Instance();
	
	// allocate memory for verts/inds
	m_pVertData = static_cast<Vertex*>(context->GetGarlic()->allocate(
		sizeof(s_quadVerts), sce::Gnm::kAlignmentOfBufferInBytes));

	m_pIndData = static_cast<uint16_t*>(context->GetGarlic()->allocate(
		sizeof(s_quadInds), sce::Gnm::kAlignmentOfBufferInBytes));


	if (!m_pVertData || !m_pIndData)
	{
		printf("failed to init vert/ind data \n");
		return -1;
	}


	// copy data into allocated memory
	memcpy(m_pVertData, s_quadVerts, sizeof(s_quadVerts));
	memcpy(m_pIndData, s_quadInds, sizeof(s_quadInds));

	m_vertexBuffers[kVertexPos].initAsVertexBuffer(
		&m_pVertData->x, //start offset
		sce::Gnm::kDataFormatR32G32B32Float,
		sizeof(Vertex), // stride
		sizeof(s_quadVerts) / sizeof(Vertex) // num of elements
	);

	m_vertexBuffers[kVertexCol].initAsVertexBuffer(
		&m_pVertData->r, //start offset
		sce::Gnm::kDataFormatR32G32B32Float,
		sizeof(Vertex), // stride
		sizeof(s_quadVerts) / sizeof(Vertex) // num of elements
	);

	m_vertexBuffers[kVertexUV].initAsVertexBuffer(
		&m_pVertData->u, //start offset
		sce::Gnm::kDataFormatR32G32B32Float,
		sizeof(Vertex), // stride
		sizeof(s_quadVerts) / sizeof(Vertex) // num of elements
	);

	// shape/mesh data end

	const char* vsPath = "/app0/myVertex_vv.sb";
	const char* psPath = "/app0/myPixel_p.sb";

	sce::Gnmx::Toolkit::Allocators* tkAlloc;
	tkAlloc = context->GetToolKitAllocs();

	m_pVsShader = loadShaderFromFile<sce::Gnmx::VsShader>(vsPath, *tkAlloc);
	m_pPsShader = loadShaderFromFile<sce::Gnmx::PsShader>(psPath, *tkAlloc);

	if (!m_pVertData || !m_pIndData)
	{
		printf("can't load the shaders \n");
		return -1;
	}


	m_fsMem = context->GetGarlic()->allocate(
		sce::Gnmx::computeVsFetchShaderSize(m_pVsShader),
		sce::Gnm::kAlignmentOfFetchShaderInBytes
	);

	if (!m_fsMem)
	{
		printf("fetch shader allocation failed\n");
		return -1;
	}

	sce::Gnm::FetchShaderInstancingMode* instancingData = NULL;
	sce::Gnmx::generateVsFetchShader(
		m_fsMem, &m_shaderModifier, m_pVsShader,
		instancingData,
		instancingData != nullptr ? 256 : 0);

	sce::Gnmx::generateInputOffsetsCache(&m_vsInputOffsetCache,
		sce::Gnm::kShaderStageVs, m_pVsShader);

	sce::Gnmx::generateInputOffsetsCache(&m_psInputOffsetCache,
		sce::Gnm::kShaderStagePs, m_pPsShader);

	return 0;
}

void Model::Render()
{
	sce::Gnmx::GnmxGfxContext& gfxc = Context::Instance()->GetRenderContext()->gfxContext;
	gfxc.setActiveShaderStages(sce::Gnm::kActiveShaderStagesVsPs);
	gfxc.setVsShader(m_pVsShader, m_shaderModifier, m_fsMem, &m_vsInputOffsetCache);
	gfxc.setPsShader(m_pPsShader, &m_psInputOffsetCache);

	ShaderConstants* constants = static_cast<ShaderConstants*>(
		gfxc.allocateFromCommandBuffer(sizeof(ShaderConstants),
			sce::Gnm::kEmbeddedDataAlignment4)
		);

	if (constants)
	{
		Matrix4 model = Matrix4::identity();

		model.setTranslation(Vector3(0, 0, z));
		z -= 0.1f;

		float aspect = 1920.0f / 1080.0f;
		Matrix4 proj = Matrix4::perspective(3.14f / 4.0f, aspect, 0.1f, 100.0f);
		Matrix4 view = Matrix4::lookAt(Point3(0, 0, -10), Point3(0, 0, 0), Vector3(0, 1, 0));

		constants->m_WorldViewProj = ToMatrix4Unaligned(proj * view * model);

		sce::Gnm::Buffer constBuffer;
		constBuffer.initAsConstantBuffer(constants, sizeof(ShaderConstants));

		gfxc.setConstantBuffers(sce::Gnm::kShaderStageVs, 0, 1, &constBuffer);

	}

	gfxc.setVertexBuffers(
		sce::Gnm::kShaderStageVs,
		0,
		eVertexElements::kVertexCount,
		m_vertexBuffers
	);

	gfxc.setIndexSize(sce::Gnm::kIndexSize16);
	gfxc.setPrimitiveType(sce::Gnm::kPrimitiveTypeTriList);

	gfxc.drawIndex(6, m_pIndData);
}
