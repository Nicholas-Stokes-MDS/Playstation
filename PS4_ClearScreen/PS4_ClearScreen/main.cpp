////
////#include <stdio.h>
////#include <stdlib.h>
////#include <scebase.h>
////#include <kernel.h>
////#include <gnmx.h>
////#include <video_out.h>
////
//////files added in
////#include "toolkit/toolkit.h" //when adding this dont forget to add as refence project!
////#include "common/allocator.h"
//#include "Context.h"
//
//typedef struct RenderContext
//{
//	sce::Gnmx::GnmxGfxContext    gfxContext;
//#if SCE_GNMX_ENABLE_GFX_LCUE
//	void* resourceBuffer;
//	void* dcbBuffer;
//#else
//	void* cueHeap;
//	void* dcbBuffer;
//	void* ccbBuffer;
//#endif
//	volatile uint32_t* contextLabel;
//}
//RenderContext;
//
//typedef struct DisplayBuffer
//{
//	sce::Gnm::RenderTarget       renderTarget;
//	int                     displayIndex;
//}
//DisplayBuffer;
//
//enum RenderContextState
//{
//	kRenderContextFree = 0,
//	kRenderContextInUse,
//};
//
//size_t sceLibcHeapSize = 64 * 1024 * 1024;
//
//
//
//int main(int argc, const char* argv[])
//{
//	//default vars
//	static  uint32_t kDisplayBufferWidth = 1920;
//	static uint32_t kDisplayBufferHeight = 1080;
//	static const uint32_t kDisplayBufferCount = 3;
//	static const uint32_t kRenderContextCount = 2;
//	static const sce::Gnm::ZFormat kZFormat = sce::Gnm::kZFormat32Float;
//	static const sce::Gnm::StencilFormat kStencilFormat = sce::Gnm::kStencil8;
//	static const bool kHtileEnabled = true;
//	static const Vector4 kClearColor = Vector4(1.0f, 0.0f, 0.0f, 1);
//	static const size_t kOnionMemorySize = 16 * 1024 * 1024;
//	static const size_t kGarlicMemorySize = 64 * 4 * 1024 * 1024;
//
//#if SCE_GNMX_ENABLE_GFX_LCUE
//	static const uint32_t kNumLcueBatches = 100;
//	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
//#else
//	static const uint32_t kCueRingEntries = 64;
//	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
//	static const size_t kCcbSizeInBytes = 256 * 1024;
//#endif
//
//
//
//	//init code start ----------------------------------------------------
//	int ret = 0; //return value
//	sce::Gnm::GpuMode gpuMode = sce::Gnm::getGpuMode();
//
//	//allocators
//	LinearAllocator onionAlloc;
//	onionAlloc.initialize(kOnionMemorySize, SCE_KERNEL_WB_ONION,
//		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);
//
//	if (ret != SCE_OK)
//		return ret;
//
//	LinearAllocator garlicAlloc;
//	ret = garlicAlloc.initialize(kGarlicMemorySize, SCE_KERNEL_WC_GARLIC,
//		SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);
//
//	if (ret != SCE_OK)
//		return ret;
//
//	int videoOutHandle = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
//	if (videoOutHandle < 0)
//	{
//		printf("sceVideoOutOpen failed: 0x%08X\n", videoOutHandle);
//		return videoOutHandle;
//	}
//
//	//creat the event queue used to sync with EOP events
//	SceKernelEqueue eopEventQueue;
//	ret = sceKernelCreateEqueue(&eopEventQueue, "EOP QUEUE");
//	if (ret != SCE_OK)
//	{
//		printf("sceKernelCreateEqueue failed: 0x%08X\n", ret);
//		return ret;
//	}
//
//	// Register for the end-of-pipe events
//	ret = sce::Gnm::addEqEvent(eopEventQueue, sce::Gnm::kEqEventGfxEop, NULL);
//	if (ret != SCE_OK)
//	{
//		printf("Gnm::addEqEvent failed: 0x%08X\n", ret);
//		return ret;
//	}
//
//	//init the toolkit module
//	sce::Gnmx::Toolkit::Allocators toolkitAllocators;
//	onionAlloc.getIAllocator(toolkitAllocators.m_onion);
//	garlicAlloc.getIAllocator(toolkitAllocators.m_garlic);
//	sce::Gnmx::Toolkit::initializeWithAllocators(&toolkitAllocators);
//
//
//	//structs/enums pasted above
//
//	RenderContext renderContexts[kRenderContextCount];
//	RenderContext* renderContext = renderContexts;
//	uint32_t renderContextIndex = 0;
//
//
//	for (uint32_t i = 0; i < kRenderContextCount; ++i)
//	{
//		//allocate CUE heap memory (Constant update engine)
//		renderContexts[i].cueHeap = garlicAlloc.allocate(
//			sce::Gnmx::ConstantUpdateEngine::computeHeapSize(kCueRingEntries),
//			sce::Gnm::kAlignmentOfBufferInBytes
//		);
//
//		if (!renderContexts[i].cueHeap)
//		{
//			printf("Cannot allocate the CUE heap memory\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//
//		//allocate the draw command buffer
//		renderContexts[i].dcbBuffer = onionAlloc.allocate(
//			kDcbSizeInBytes,
//			sce::Gnm::kAlignmentOfBufferInBytes
//		);
//		if (!renderContexts[i].dcbBuffer)
//		{
//			printf("Cannot allocate the draw command buffer memory\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//
//		//allocate the constants command buffer
//		renderContexts[i].ccbBuffer = onionAlloc.allocate(
//			kCcbSizeInBytes,
//			sce::Gnm::kAlignmentOfBufferInBytes);
//		if (!renderContexts[i].ccbBuffer)
//		{
//			printf("Cannot allocate the constants command buffer memory\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//
//		renderContexts[i].gfxContext.init(
//			renderContexts[i].cueHeap,
//			kCueRingEntries,
//			renderContexts[i].dcbBuffer,
//			kDcbSizeInBytes,
//			renderContexts[i].ccbBuffer,
//			kCcbSizeInBytes);
//
//		renderContexts[i].contextLabel = (volatile uint32_t*)onionAlloc.allocate(4, 8);
//		if (!renderContexts[i].contextLabel)
//		{
//			printf("Cannot allocate a GPU label\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//
//		renderContexts[i].contextLabel[0] = kRenderContextFree;
//	}
//
//	DisplayBuffer displayBuffers[kDisplayBufferCount];
//	DisplayBuffer* backBuffer = displayBuffers;
//	uint32_t backBufferIndex = 0;
//
//	// Convenience array used by sceVideoOutRegisterBuffers()
//	void * surfaceAddresses[kDisplayBufferCount];
//
//	SceVideoOutResolutionStatus status;
//	if (SCE_OK == sceVideoOutGetResolutionStatus(videoOutHandle, &status) && status.fullHeight > 1080)
//	{
//		kDisplayBufferWidth *= 2;
//		kDisplayBufferHeight *= 2;
//	}
//
//	for (uint32_t i = 0; i < kDisplayBufferCount; ++i)
//	{
//		sce::Gnm::TileMode tileMode;
//		sce::Gnm::DataFormat format = sce::Gnm::kDataFormatB8G8R8A8UnormSrgb;
//
//		ret = sce::GpuAddress::computeSurfaceTileMode(gpuMode, &tileMode,
//			sce::GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1
//		);
//
//		if (ret != SCE_OK)
//		{
//			printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
//			return ret;
//		}
//
//		sce::Gnm::RenderTargetSpec spec;
//		spec.init();
//		spec.m_width = kDisplayBufferWidth;
//		spec.m_height = kDisplayBufferHeight;
//		spec.m_pitch = 0;
//		spec.m_numSlices = 1;
//		spec.m_colorFormat = format;
//		spec.m_colorTileModeHint = tileMode;
//		spec.m_minGpuMode = gpuMode;
//		spec.m_numSamples = sce::Gnm::kNumSamples1;
//		spec.m_numFragments = sce::Gnm::kNumFragments1;
//		spec.m_flags.enableCmaskFastClear = 0;
//		spec.m_flags.enableFmaskCompression = 0;
//		ret = displayBuffers[i].renderTarget.init(&spec);
//		if (ret != SCE_GNM_OK)
//			return ret;
//		
//		sce::Gnm::SizeAlign sizeAlign = displayBuffers[i].renderTarget.getColorSizeAlign();
//		//allocate the render target mem
//		surfaceAddresses[i] = garlicAlloc.allocate(sizeAlign);
//		if (!surfaceAddresses[i])
//		{
//			printf("Cannot allocate the render target memory\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//		displayBuffers[i].renderTarget.setAddresses(surfaceAddresses[i], 0, 0);
//
//		displayBuffers[i].displayIndex = i;
//	}
//
//	// Initialization the VideoOut buffer descriptor. The pixel format must
//	// match with the render target data format
//	SceVideoOutBufferAttribute videoOutBuffAtt;
//	sceVideoOutSetBufferAttribute(
//		&videoOutBuffAtt,
//		SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
//		SCE_VIDEO_OUT_TILING_MODE_TILE,
//		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
//		backBuffer->renderTarget.getWidth(),
//		backBuffer->renderTarget.getHeight(),
//		backBuffer->renderTarget.getPitch()
//	);
//
//	// Register the buffers to the slot: [0..kDisplayBufferCount-1]
//	ret = sceVideoOutRegisterBuffers(
//		videoOutHandle,
//		0, // Start index
//		surfaceAddresses,
//		kDisplayBufferCount,
//		&videoOutBuffAtt
//	);
//	if (ret != SCE_OK)
//	{
//		printf("sceVideoOutRegisterBuffers failed: 0x%08X\n", ret);
//		return ret;
//	}
//
//	// Compute the tiling mode for the depth buffer
//	sce::Gnm::DataFormat depthFormat = sce::Gnm::DataFormat::build(kZFormat);
//	sce::Gnm::TileMode depthTileMode;
//	ret = sce::GpuAddress::computeSurfaceTileMode(
//		gpuMode, // NEO or Base
//		&depthTileMode,									// Tile mode pointer
//		sce::GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
//		depthFormat,									// Surface format
//		1);												// Elements per pixel
//	if (ret != SCE_OK)
//	{
//		printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
//		return ret;
//	}
//
//	sce::Gnm::DepthRenderTarget depthTarget; // this will be needed later. 
//	sce::Gnm::SizeAlign stencilSizeAlign;
//	sce::Gnm::SizeAlign htileSizeAlign;
//
//
//	sce::Gnm::DepthRenderTargetSpec spec;
//	spec.init();
//	spec.m_width = kDisplayBufferWidth;
//	spec.m_height = kDisplayBufferHeight;
//	spec.m_pitch = 0;
//	spec.m_numSlices = 1;
//	spec.m_zFormat = depthFormat.getZFormat();
//	spec.m_stencilFormat = kStencilFormat;
//	spec.m_minGpuMode = gpuMode;
//	spec.m_numFragments = sce::Gnm::kNumFragments1;
//	spec.m_flags.enableHtileAcceleration = kHtileEnabled ? 1 : 0;
//
//	ret = depthTarget.init(&spec);
//	if (ret != SCE_GNM_OK)
//		return ret;
//
//	sce::Gnm::SizeAlign depthTargetSizeAlign = depthTarget.getZSizeAlign();
//	// Initialize the HTILE buffer, if enabled
//	if (kHtileEnabled)
//	{
//		htileSizeAlign = depthTarget.getHtileSizeAlign();
//		void* htileMemory = garlicAlloc.allocate(htileSizeAlign);
//		if (!htileMemory)
//		{
//			printf("Cannot allocate the HTILE buffer\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//
//		depthTarget.setHtileAddress(htileMemory);
//	}
//
//	void* stencilMem = NULL;
//	stencilSizeAlign = depthTarget.getStencilSizeAlign();
//	if (kStencilFormat != sce::Gnm::kStencilInvalid)
//	{
//		stencilMem = garlicAlloc.allocate(stencilSizeAlign);
//		if (!stencilMem)
//		{
//			printf("Cannot allocate the stencil buffer\n");
//			return SCE_KERNEL_ERROR_ENOMEM;
//		}
//	}
//
//	// Allocate the depth buffer
//	void* depthMem = garlicAlloc.allocate(depthTargetSizeAlign);
//	if (!depthMem)
//	{
//		printf("Cannot allocate the depth buffer\n");
//		return SCE_KERNEL_ERROR_ENOMEM;
//	}
//	depthTarget.setAddresses(depthMem, stencilMem);
//
//
//	//END OF INIT --------------------------------------------------
//
//
//
//	//1000 frames or 10 seconds
//	for (int i = 0; i < 600; i++)
//	{
//		//BEGIN FRAME -------------------------------------------------
//		sce::Gnmx::GnmxGfxContext& gfxc = renderContext->gfxContext;
//
//		//wait for the context label to be free
//		while (renderContext->contextLabel[0] != kRenderContextFree)
//		{
//			SceKernelEvent eopEvent;
//			int count;
//			ret = sceKernelWaitEqueue(eopEventQueue, &eopEvent, 1, &count, NULL);
//			if (ret != SCE_OK)
//			{
//				printf("sceKernelWaitEqueue failed: 0x%08X\n", ret);
//			}
//		}
//
//		// Reset the flip GPU label
//		renderContext->contextLabel[0] = kRenderContextInUse;
//
//		gfxc.reset();
//		gfxc.initializeDefaultHardwareState();
//
//		//use right before writing to display buffer
//		gfxc.waitUntilSafeForRendering(videoOutHandle, backBuffer->displayIndex);
//
//		//set up the viewport to match the screen
//		gfxc.setupScreenViewport(0, 0, backBuffer->renderTarget.getWidth(), backBuffer->renderTarget.getHeight(), 0.5f, 0.5f);
//
//		// Clear the color and the depth target
//		sce::Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(gfxc, &backBuffer->renderTarget, kClearColor);
//		sce::Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(gfxc, &depthTarget, 1.0f);
//
//		//BEGIN FRAME END -------------------------------------------------
//		printf("current frame %d\n", i);
//
//		//END FRAME ------------------------------------------------------
//
//		// Submit the command buffers, request a flip of the display buffer and
//		// write the GPU label that determines the render context state (free)
//		// and trigger a software interrupt to signal the EOP event queue
//		ret = gfxc.submitAndFlipWithEopInterrupt(
//			videoOutHandle,
//			backBuffer->displayIndex,
//			SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
//			0,
//			sce::Gnm::kEopFlushCbDbCaches,
//			const_cast<uint32_t*>(renderContext->contextLabel),
//			kRenderContextFree,
//			sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2
//		);
//
//		if (ret != sce::Gnm::kSubmissionSuccess)
//		{
//			if (ret & sce::Gnm::kStatusMaskError)
//			{
//				renderContext->contextLabel[0] = kRenderContextFree;
//			}
//			printf("GFxContext::submitandFLip failed: 0x%0X\n", ret);
//
//		}
//
//		// Signal the system that every draw for this frame has been submitted.
//		// This function gives permission to the OS to hibernate when all the
//		// currently running GPU tasks (graphics and compute) are done.
//		ret = sce::Gnm::submitDone();
//		if (ret != SCE_OK)
//		{
//			printf("Gnm::submitDone failed: 0x%08X\n", ret);
//		}
//
//		// Rotate the display buffers
//		backBufferIndex = (backBufferIndex + 1) % kDisplayBufferCount;
//		backBuffer = displayBuffers + backBufferIndex;
//
//		// Rotate the render contexts
//		renderContextIndex = (renderContextIndex + 1) % kRenderContextCount;
//		renderContext = renderContexts + renderContextIndex;
//
//		//ENDFRAME END ---------------------------------------------------------
//	}
//
//
//	//SHUTDOWN ----------------------------------------------------------------
//
//		// Wait for the GPU to be idle before deallocating its resources
//	for (uint32_t i = 0; i < kRenderContextCount; ++i)
//	{
//		if (renderContexts[i].contextLabel)
//		{
//			while (renderContexts[i].contextLabel[0] != kRenderContextFree)
//			{
//				sceKernelUsleep(1000);
//			}
//		}
//	}
//
//	// Unregister the EOP event queue
//	ret = sce::Gnm::deleteEqEvent(eopEventQueue, sce::Gnm::kEqEventGfxEop);
//	if (ret != SCE_OK)
//	{
//		printf("Gnm::deleteEqEvent failed: 0x%08X\n", ret);
//	}
//
//	// Destroy the EOP event queue
//	ret = sceKernelDeleteEqueue(eopEventQueue);
//	if (ret != SCE_OK)
//	{
//		printf("sceKernelDeleteEqueue failed: 0x%08X\n", ret);
//	}
//
//	// Terminate the video output
//	ret = sceVideoOutClose(videoOutHandle);
//	if (ret != SCE_OK)
//	{
//		printf("sceVideoOutClose failed: 0x%08X\n", ret);
//	}
//
//	// Releasing manually each allocated resource is not necessary as we are
//	// terminating the linear allocators for ONION and GARLIC here.
//	onionAlloc.terminate();
//	garlicAlloc.terminate();
//
//	//SHUTDOWN END ---------------------------------------------------------
//
//
//	return 0;
//}

#include "Context.h"


size_t sceLibcHeapSize = 64 * 1024 * 1024;

Context* context;

int main(int argc, const char* argv[])
{
	context = new Context();
	context->Initialise();

	for (int i = 0; i < 600; i++)
	{
		context->BeginFrame();

		context->EndFrame();

		printf("current frame %d\n", i);
	}

	context->Shutdown();
	delete context;
	context = nullptr;

	return 0;
}