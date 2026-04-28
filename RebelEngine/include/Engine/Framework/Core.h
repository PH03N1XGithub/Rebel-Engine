#pragma once
#ifdef REBELENGINE_DLL
#define REBELENGINE_API //__declspec(dllexport)
#else
#define REBELENGINE_API //__declspec(dllimport)
#endif



