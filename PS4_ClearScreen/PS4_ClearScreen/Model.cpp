#include "Model.h"
#include "common/shader_loader.h"
#include "Context.h"

#include "std_cbuffer.h"

#include "common/texture_util.h"

//Vertex s_quadVerts[4] =
//{
//	// position				// colour			// UV
// {-0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    0.0f, 1.0f},
// { 0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    1.0f, 1.0f},
// {-0.5f,  0.5f, 0.0f,    0.7f, 1.0f, 1.0f,    0.0f, 0.0f},
// { 0.5f,  0.5f, 0.0f,    1.0f, 0.7f, 1.0f,    1.0f, 0.0f},
//};

Vertex s_quadVerts[8] =
{
// position               // color             // UV (arbitrary, can be adjusted per-face)
	{-0.5f, -0.5f, -0.5f,     1.0f, 0.0f, 0.0f,    0.0f, 1.0f}, // 0 - left bottom back
	{ 0.5f, -0.5f, -0.5f,     0.0f, 1.0f, 0.0f,    1.0f, 1.0f}, // 1 - right bottom back
	{-0.5f,  0.5f, -0.5f,     0.0f, 0.0f, 1.0f,    0.0f, 0.0f}, // 2 - left top back
	{ 0.5f,  0.5f, -0.5f,     0.0f, 0.0f, 0.0f,    1.0f, 0.0f}, // 3 - right top back
	{-0.5f, -0.5f,  0.5f,     1.0f, 1.0f, 0.0f,    0.0f, 1.0f}, // 4 - left bottom front
	{ 0.5f, -0.5f,  0.5f,     0.0f, 1.0f, 1.0f,    1.0f, 1.0f}, // 5 - right bottom front
	{-0.5f,  0.5f,  0.5f,     1.0f, 0.0f, 1.0f,    0.0f, 0.0f}, // 6 - left top front
	{ 0.5f,  0.5f,  0.5f,     1.0f, 1.0f, 1.0f,    1.0f, 0.0f}, // 7 - right top front
};

uint16_t s_quadInds[36] =
{
	// back face
	0, 1, 2,
	1, 3, 2,

	// front face
	4, 6, 5,
	5, 6, 7,

	// left face
	0, 2, 4,
	4, 2, 6,

	// right face
	1, 5, 3,
	5, 7, 3,

	// bottom face
	0, 4, 1,
	1, 4, 5,

	// top face
	2, 3, 6,
	6, 3, 7
};

//uint16_t s_quadInds[6] =
//{
// 0, 1, 2,
// 1, 3, 2
//};

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

	const char* _txtrPath = "/app0/textures/shuckle2.gnf";

	TextureUtil::loadTextureFromGnf(&m_texture, _txtrPath, 0, *tkAlloc);

	// initialize the texture sampler
	m_sampler.init();
	m_sampler.setMipFilterMode(sce::Gnm::kMipFilterModeNone);
	m_sampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear, sce::Gnm::kFilterModeBilinear);


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
		Matrix4 model =
		{
				{  0.7071f, 0,  0.7071f, 0 },
				{  0,       1,  0,       0 },
				{ -0.7071f, 0,  0.7071f, 0 },
				{  0,       0,  0,       1 }
		};

			//Matrix4::identity();
		//Matrix4 translation = Matrix4::identity();
		//Matrix4 rotation = Matrix4::identity();
		//Matrix4 scale = Matrix4::identity();

		//model.setTranslation(Vector3(z, 0, -5));
		//z -= 0.1f;

		//model = Matrix4::Rot

		//rotation = Matrix4::rotation()

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

	gfxc.setTextures(sce::Gnm::kShaderStagePs, 0, 1, &m_texture);
	gfxc.setSamplers(sce::Gnm::kShaderStagePs, 0, 1, &m_sampler);

	gfxc.setIndexSize(sce::Gnm::kIndexSize16);
	gfxc.setPrimitiveType(sce::Gnm::kPrimitiveTypeTriList);

	gfxc.drawIndex(sizeof(s_quadInds), m_pIndData);
}
