#include "EnginePch.h"
#include "EditorEngine.h"



int main()
{
	std::cout << "Editor running..." << std::endl;
	GEngine = new EditorEngine();
	GEngine->Run();
	delete GEngine;
	return 0;
}