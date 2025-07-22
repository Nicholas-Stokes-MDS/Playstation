#pragma once


#include <stdio.h>
#include <stdlib.h>
#include <scebase.h>
#include <kernel.h>
#include <gnmx.h>
#include <video_out.h>

#include "toolkit/toolkit.h" //when adding this dont forget to add as refence project!
#include "common/allocator.h"


typedef struct RenderContext
{
	sce::Gnmx::GnmxGfxContext    gfxContext;
#if SCE_GNMX_ENABLE_GFX_LCUE
	void* resourceBuffer;
	void* dcbBuffer;
#else
	void* cueHeap;
	void* dcbBuffer;
	void* ccbBuffer;
#endif
	volatile uint32_t* contextLabel;
}
RenderContext;

typedef struct DisplayBuffer
{
	sce::Gnm::RenderTarget       renderTarget;
	int                     displayIndex;
}
DisplayBuffer;

enum RenderContextState
{
	kRenderContextFree = 0,
	kRenderContextInUse,
};


class Context
{

private:

	//default vars

	static const uint32_t kDisplayBufferCount = 3;
	static const uint32_t kRenderContextCount = 2;
	static const sce::Gnm::ZFormat kZFormat = sce::Gnm::kZFormat32Float;
	static const sce::Gnm::StencilFormat kStencilFormat = sce::Gnm::kStencil8;
	static const bool kHtileEnabled = true;
	const Vector4 kClearColor = Vector4(1.0f, 0.0f, 0.0f, 1);
	static const size_t kOnionMemorySize = 16 * 1024 * 1024;
	static const size_t kGarlicMemorySize = 64 * 4 * 1024 * 1024;

#if SCE_GNMX_ENABLE_GFX_LCUE
	static const uint32_t kNumLcueBatches = 100;
	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
#else
	static const uint32_t kCueRingEntries = 64;
	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
	static const size_t kCcbSizeInBytes = 256 * 1024;
#endif

	RenderContext* renderContext;
	uint32_t renderContextIndex = 0;
	uint32_t backBufferIndex = 0;

	SceKernelEqueue eopEventQueue;

	int videoOutHandle;

	DisplayBuffer* backBuffer;

	sce::Gnm::DepthRenderTarget depthTarget;

	sce::Gnmx::GnmxGfxContext* gfxc;

	int ret;

	LinearAllocator onionAlloc;

	LinearAllocator garlicAlloc;

	DisplayBuffer displayBuffers[kDisplayBufferCount];

	RenderContext renderContexts[kRenderContextCount];

	uint32_t kDisplayBufferWidth = 1920;
	uint32_t kDisplayBufferHeight = 1080;

public:

	Context();
	~Context();

	int Initialise();
	void BeginFrame();
	void EndFrame();
	void Shutdown();

};

