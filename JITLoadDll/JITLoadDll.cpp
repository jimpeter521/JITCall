#include "JITCall.hpp"
#include "CommandParser.hpp"
#include "MainWindow.h"

#include <stdio.h>

int test(int i, float j) {
	printf("One:%d Two:%f\n", i, j);
	return 0;
}

int main()
{
	Command cmdParser(GetCommandLineA());
	printf("%s\n", GetCommandLineA());
	if (cmdParser.GetArgCount()) {
		std::string self = cmdParser.GetText();
		printf("Arg 0: %s\n", self.c_str());
		for (int i = 0; i < cmdParser.GetArgCount(); i++) {
			printf("Arg %d: %s\n", i + 1, cmdParser.GetArg(i).c_str());
		}
	}

	MainWindow window;
	window.InitWindow();

	JITCall jit((char*)&test);
	const uint8_t paramCount = window.getParamCount();
	std::vector<std::string> paramTypes;
	JITCall::Parameters* params = (JITCall::Parameters*)(char*)new uint64_t[paramCount];

	// build param list of types from GUI state
	for (uint8_t i = 0; i < paramCount; i++) {
		auto pstate = window.getParamState(i);
		paramTypes.push_back(pstate.type);
		*(uint64_t*)params->getArgPtr(i) = pstate.getPacked();
	}

	JITCall::tJitCall pCall = jit.getJitFunc("int", paramTypes);
	pCall(params);
	delete[] params;
	getchar();
}

