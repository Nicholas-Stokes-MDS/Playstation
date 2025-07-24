#include "Context.h"
#include "Model.h"


size_t sceLibcHeapSize = 64 * 1024 * 1024;

Context* context;

int main(int argc, const char* argv[])
{
	//context = new Context();

	Context* context = Context::Instance();

	Model model;

	context->Initialise();

	if (model.Init() != 0)
	{
		printf("model init failed\n");
	};


	for (int i = 0; i < 600; i++)
	{
		context->BeginFrame();

		context->SetPipeline(true, true, true);

		model.Render();

		context->EndFrame();

		printf("current frame %d\n", i);
	}

	context->Shutdown();

	context->DestroyInstance();

	//delete context;
	context = nullptr;

	

	return 0;
}