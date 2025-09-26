#include <iostream>

// forward declaration
void EngineTest();

int main()
{
	std::cout << "Editor running..." << std::endl;
	EngineTest();  // call function from RebelEngine DLL
	return 0;
}