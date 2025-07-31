#include "Context.h"

Context::Context()
{

}

Context::~Context()
{

}

Context* Context::s_pInstance = nullptr;

Context* Context::Instance()
{
	if (s_pInstance == nullptr)
	{
		s_pInstance = new Context();
	}

	return s_pInstance;
}

void Context::DestroyInstance()
{
	if (s_pInstance != nullptr)
	{
		delete s_pInstance;
		s_pInstance = nullptr;
	}
}

int Context::Initialise()
{
	//init code start ----------------------------------------------------
	ret = 0;
	sce::Gnm::GpuMode gpuMode = sce::Gnm::getGpuMode();

	//allocators
	onionAlloc.initialize(kOnionMemorySize, SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);

	if (ret != SCE_OK)
		return ret;

	ret = garlicAlloc.initialize(kGarlicMemorySize, SCE_KERNEL_WC_GARLIC,
		SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);

	if (ret != SCE_OK)
		return ret;

	videoOutHandle = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	if (videoOutHandle < 0)
	{
		printf("sceVideoOutOpen failed: 0x%08X\n", videoOutHandle);
		return videoOutHandle;
	}

	//creat the event queue used to sync with EOP events
	ret = sceKernelCreateEqueue(&eopEventQueue, "EOP QUEUE");
	if (ret != SCE_OK)
	{
		printf("sceKernelCreateEqueue failed: 0x%08X\n", ret);
		return ret;
	}

	// Register for the end-of-pipe events
	ret = sce::Gnm::addEqEvent(eopEventQueue, sce::Gnm::kEqEventGfxEop, NULL);
	if (ret != SCE_OK)
	{
		printf("Gnm::addEqEvent failed: 0x%08X\n", ret);
		return ret;
	}

	//init the toolkit module

	onionAlloc.getIAllocator(toolkitAllocators.m_onion);
	garlicAlloc.getIAllocator(toolkitAllocators.m_garlic);
	sce::Gnmx::Toolkit::initializeWithAllocators(&toolkitAllocators);


	//structs/enums pasted above

	
	renderContext = renderContexts;
	renderContextIndex = 0;


	for (uint32_t i = 0; i < kRenderContextCount; ++i)
	{
		//allocate CUE heap memory (Constant update engine)
		renderContexts[i].cueHeap = garlicAlloc.allocate(
			sce::Gnmx::ConstantUpdateEngine::computeHeapSize(kCueRingEntries),
			sce::Gnm::kAlignmentOfBufferInBytes
		);

		if (!renderContexts[i].cueHeap)
		{
			printf("Cannot allocate the CUE heap memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		//allocate the draw command buffer
		renderContexts[i].dcbBuffer = onionAlloc.allocate(
			kDcbSizeInBytes,
			sce::Gnm::kAlignmentOfBufferInBytes
		);
		if (!renderContexts[i].dcbBuffer)
		{
			printf("Cannot allocate the draw command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		//allocate the constants command buffer
		renderContexts[i].ccbBuffer = onionAlloc.allocate(
			kCcbSizeInBytes,
			sce::Gnm::kAlignmentOfBufferInBytes);
		if (!renderContexts[i].ccbBuffer)
		{
			printf("Cannot allocate the constants command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		renderContexts[i].gfxContext.init(
			renderContexts[i].cueHeap,
			kCueRingEntries,
			renderContexts[i].dcbBuffer,
			kDcbSizeInBytes,
			renderContexts[i].ccbBuffer,
			kCcbSizeInBytes);

		renderContexts[i].contextLabel = (volatile uint32_t*)onionAlloc.allocate(4, 8);
		if (!renderContexts[i].contextLabel)
		{
			printf("Cannot allocate a GPU label\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		renderContexts[i].contextLabel[0] = kRenderContextFree;
	}

	//DisplayBuffer displayBuffers[kDisplayBufferCount];
	backBuffer = displayBuffers;
	backBufferIndex = 0;

	// Convenience array used by sceVideoOutRegisterBuffers()
	void* surfaceAddresses[kDisplayBufferCount];

	SceVideoOutResolutionStatus status;
	if (SCE_OK == sceVideoOutGetResolutionStatus(videoOutHandle, &status) && status.fullHeight > 1080)
	{
		kDisplayBufferWidth *= 2;
		kDisplayBufferHeight *= 2;
	}

	for (uint32_t i = 0; i < kDisplayBufferCount; ++i)
	{
		sce::Gnm::TileMode tileMode;
		sce::Gnm::DataFormat format = sce::Gnm::kDataFormatB8G8R8A8UnormSrgb;

		ret = sce::GpuAddress::computeSurfaceTileMode(gpuMode, &tileMode,
			sce::GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1
		);

		if (ret != SCE_OK)
		{
			printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
			return ret;
		}

		sce::Gnm::RenderTargetSpec spec;
		spec.init();
		spec.m_width = kDisplayBufferWidth;
		spec.m_height = kDisplayBufferHeight;
		spec.m_pitch = 0;
		spec.m_numSlices = 1;
		spec.m_colorFormat = format;
		spec.m_colorTileModeHint = tileMode;
		spec.m_minGpuMode = gpuMode;
		spec.m_numSamples = sce::Gnm::kNumSamples1;
		spec.m_numFragments = sce::Gnm::kNumFragments1;
		spec.m_flags.enableCmaskFastClear = 0;
		spec.m_flags.enableFmaskCompression = 0;
		ret = displayBuffers[i].renderTarget.init(&spec);
		if (ret != SCE_GNM_OK)
			return ret;

		sce::Gnm::SizeAlign sizeAlign = displayBuffers[i].renderTarget.getColorSizeAlign();
		//allocate the render target mem
		surfaceAddresses[i] = garlicAlloc.allocate(sizeAlign);
		if (!surfaceAddresses[i])
		{
			printf("Cannot allocate the render target memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
		displayBuffers[i].renderTarget.setAddresses(surfaceAddresses[i], 0, 0);

		displayBuffers[i].displayIndex = i;
	}

	// Initialization the VideoOut buffer descriptor. The pixel format must
	// match with the render target data format
	SceVideoOutBufferAttribute videoOutBuffAtt;
	sceVideoOutSetBufferAttribute(
		&videoOutBuffAtt,
		SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		backBuffer->renderTarget.getWidth(),
		backBuffer->renderTarget.getHeight(),
		backBuffer->renderTarget.getPitch()
	);

	// Register the buffers to the slot: [0..kDisplayBufferCount-1]
	ret = sceVideoOutRegisterBuffers(
		videoOutHandle,
		0, // Start index
		surfaceAddresses,
		kDisplayBufferCount,
		&videoOutBuffAtt
	);
	if (ret != SCE_OK)
	{
		printf("sceVideoOutRegisterBuffers failed: 0x%08X\n", ret);
		return ret;
	}

	// Compute the tiling mode for the depth buffer
	sce::Gnm::DataFormat depthFormat = sce::Gnm::DataFormat::build(kZFormat);
	sce::Gnm::TileMode depthTileMode;
	ret = sce::GpuAddress::computeSurfaceTileMode(
		gpuMode, // NEO or Base
		&depthTileMode,									// Tile mode pointer
		sce::GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
		depthFormat,									// Surface format
		1);												// Elements per pixel
	if (ret != SCE_OK)
	{
		printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
		return ret;
	}


	sce::Gnm::SizeAlign stencilSizeAlign;
	sce::Gnm::SizeAlign htileSizeAlign;


	sce::Gnm::DepthRenderTargetSpec spec;
	spec.init();
	spec.m_width = kDisplayBufferWidth;
	spec.m_height = kDisplayBufferHeight;
	spec.m_pitch = 0;
	spec.m_numSlices = 1;
	spec.m_zFormat = depthFormat.getZFormat();
	spec.m_stencilFormat = kStencilFormat;
	spec.m_minGpuMode = gpuMode;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	spec.m_flags.enableHtileAcceleration = kHtileEnabled ? 1 : 0;

	ret = depthTarget.init(&spec);
	if (ret != SCE_GNM_OK)
		return ret;

	sce::Gnm::SizeAlign depthTargetSizeAlign = depthTarget.getZSizeAlign();
	// Initialize the HTILE buffer, if enabled
	if (kHtileEnabled)
	{
		htileSizeAlign = depthTarget.getHtileSizeAlign();
		void* htileMemory = garlicAlloc.allocate(htileSizeAlign);
		if (!htileMemory)
		{
			printf("Cannot allocate the HTILE buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		depthTarget.setHtileAddress(htileMemory);
	}

	void* stencilMem = NULL;
	stencilSizeAlign = depthTarget.getStencilSizeAlign();
	if (kStencilFormat != sce::Gnm::kStencilInvalid)
	{
		stencilMem = garlicAlloc.allocate(stencilSizeAlign);
		if (!stencilMem)
		{
			printf("Cannot allocate the stencil buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
	}

	// Allocate the depth buffer
	void* depthMem = garlicAlloc.allocate(depthTargetSizeAlign);
	if (!depthMem)
	{
		printf("Cannot allocate the depth buffer\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}
	depthTarget.setAddresses(depthMem, stencilMem);

	return 0;


}

void Context::BeginFrame()
{
	// BEGIN FRAME------------------------------------------------ -
	sce::Gnmx::GnmxGfxContext& gfxc = renderContext->gfxContext;

	//wait for the context label to be free
	while (renderContext->contextLabel[0] != kRenderContextFree)
	{
		SceKernelEvent eopEvent;
		int count;
		ret = sceKernelWaitEqueue(eopEventQueue, &eopEvent, 1, &count, NULL);
		if (ret != SCE_OK)
		{
			printf("sceKernelWaitEqueue failed: 0x%08X\n", ret);
		}
	}

	// Reset the flip GPU label
	renderContext->contextLabel[0] = kRenderContextInUse;

	gfxc.reset();
	gfxc.initializeDefaultHardwareState();

	//use right before writing to display buffer
	gfxc.waitUntilSafeForRendering(videoOutHandle, backBuffer->displayIndex);

	//set up the viewport to match the screen
	gfxc.setupScreenViewport(0, 0, backBuffer->renderTarget.getWidth(), backBuffer->renderTarget.getHeight(), 0.5f, 0.5f);

	gfxc.setRenderTarget(0, &backBuffer->renderTarget);
	gfxc.setDepthRenderTarget(&depthTarget);

	// Clear the color and the depth target
	sce::Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(gfxc, &backBuffer->renderTarget, kClearColor);
	sce::Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(gfxc, &depthTarget, 1.0f);

	//BEGIN FRAME END -------------------------------------------------
}

void Context::EndFrame()
{
	//END FRAME ------------------------------------------------------

		// Submit the command buffers, request a flip of the display buffer and
		// write the GPU label that determines the render context state (free)
		// and trigger a software interrupt to signal the EOP event queue

	sce::Gnmx::GnmxGfxContext& gfxc = renderContext->gfxContext;

	ret = gfxc.submitAndFlipWithEopInterrupt(
		videoOutHandle,
		backBuffer->displayIndex,
		SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
		0,
		sce::Gnm::kEopFlushCbDbCaches,
		const_cast<uint32_t*>(renderContext->contextLabel),
		kRenderContextFree,
		sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2
	);

	if (ret != sce::Gnm::kSubmissionSuccess)
	{
		if (ret & sce::Gnm::kStatusMaskError)
		{
			renderContext->contextLabel[0] = kRenderContextFree;
		}
		printf("GFxContext::submitandFLip failed: 0x%0X\n", ret);

	}

	// Signal the system that every draw for this frame has been submitted.
	// This function gives permission to the OS to hibernate when all the
	// currently running GPU tasks (graphics and compute) are done.
	ret = sce::Gnm::submitDone();
	if (ret != SCE_OK)
	{
		printf("Gnm::submitDone failed: 0x%08X\n", ret);
	}

	// Rotate the display buffers
	backBufferIndex = (backBufferIndex + 1) % kDisplayBufferCount;
	backBuffer = displayBuffers + backBufferIndex;

	// Rotate the render contexts
	renderContextIndex = (renderContextIndex + 1) % kRenderContextCount;
	renderContext = renderContexts + renderContextIndex;

	//ENDFRAME END ---------------------------------------------------------
}

void Context::Shutdown()
{
	// Wait for the GPU to be idle before deallocating its resources
	for (uint32_t i = 0; i < kRenderContextCount; ++i)
	{
		if (renderContexts[i].contextLabel)
		{
			while (renderContexts[i].contextLabel[0] != kRenderContextFree)
			{
				sceKernelUsleep(1000);
			}
		}
	}

	// Unregister the EOP event queue
	ret = sce::Gnm::deleteEqEvent(eopEventQueue, sce::Gnm::kEqEventGfxEop);
	if (ret != SCE_OK)
	{
		printf("Gnm::deleteEqEvent failed: 0x%08X\n", ret);
	}

	// Destroy the EOP event queue
	ret = sceKernelDeleteEqueue(eopEventQueue);
	if (ret != SCE_OK)
	{
		printf("sceKernelDeleteEqueue failed: 0x%08X\n", ret);
	}

	// Terminate the video output
	ret = sceVideoOutClose(videoOutHandle);
	if (ret != SCE_OK)
	{
		printf("sceVideoOutClose failed: 0x%08X\n", ret);
	}

	// Releasing manually each allocated resource is not necessary as we are
	// terminating the linear allocators for ONION and GARLIC here.
	onionAlloc.terminate();
	garlicAlloc.terminate();

	//SHUTDOWN END ---------------------------------------------------------
}

LinearAllocator* Context::GetGarlic()
{
	return &garlicAlloc;
}

RenderContext* Context::GetRenderContext()
{
	return renderContext;
}

sce::Gnmx::Toolkit::Allocators* Context::GetToolKitAllocs()
{
	return &toolkitAllocators;
}

void Context::SetPipeline(bool _depth, bool _cull, bool _blend)
{
	sce::Gnmx::GnmxGfxContext& gfxc = renderContext->gfxContext;
	
	
	if (_depth)
	{
		sce::Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(sce::Gnm::kDepthControlZWriteEnable, sce::Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc.setDepthStencilControl(dsc);
	}

	if (_cull)
	{
		// Cull clock-wise backfaces
		sce::Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(sce::Gnm::kPrimitiveSetupCullFaceNone);
		primSetupReg.setFrontFace(sce::Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc.setPrimitiveSetup(primSetupReg);
	}

	if (_blend)
	{
		// Setup an additive blending mode
		sce::Gnm::BlendControl blendControl;
		blendControl.init();
		blendControl.setBlendEnable(true);
		blendControl.setColorEquation(
			sce::Gnm::kBlendMultiplierSrcAlpha,
			sce::Gnm::kBlendFuncAdd,
			sce::Gnm::kBlendMultiplierOneMinusSrcAlpha);
		gfxc.setBlendControl(0, blendControl);
	}

	gfxc.setRenderTargetMask(0xF);
}


