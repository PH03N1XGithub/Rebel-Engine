
#ifdef REBELENGINE_DLL
#define REBELENGINE_API __declspec(dllexport)
#else
#define REBELENGINE_API __declspec(dllimport)
#endif

#include "Core/CoreTypes.h"
#include <iostream>

void REBELENGINE_API EngineTest()  
{

	Rebel::Core::TestCore();
}