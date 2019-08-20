#include "JITCall.hpp"
#include "CommandParser.hpp"
#include "MainWindow.hpp"

#include <stdio.h>

int test(int i, float j) {
	printf("One:%d Two:%f\n", i, j);
	return 0;
}

int main()
{
	Command cmdParser(GetCommandLineA());
	std::vector<JITCall::tJitCall> jitCalls;
	std::string dllPath = "";
	MainWindow window;
	window.OnFileChosen([&](std::string path) {
		std::cout << "File Chosen: " << path << std::endl;
		dllPath = path;
	});

	window.OnNewFunction([&](const std::vector<FunctionEditor::State::ParamState>& paramStates, const char* retType) {
		JITCall jit((char*)&test);
		std::vector<std::string> paramTypes;
		JITCall::Parameters* params = (JITCall::Parameters*)(char*)new uint64_t[paramStates.size()];
		memset(params, 0, sizeof(uint64_t) * paramStates.size());

		// build param list of types from GUI state
		for (uint8_t i = 0; i < paramStates.size(); i++) {
			auto pstate = paramStates.at(i);
			paramTypes.push_back(pstate.type);
			*(uint64_t*)params->getArgPtr(i) = pstate.getPacked();
		}

		JITCall::tJitCall pCall = jit.getJitFunc(retType, paramTypes);	
		jitCalls.push_back(pCall);

		std::cout << "Added a new JIT call" << std::endl;
	});

	window.InitWindow();

	
	//pCall(params);
	//delete[] params;
	getchar();
}

