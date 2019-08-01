#include "JITCall.hpp"
#include "CommandParser.hpp"
#include "MenuGui.h"

#include <stdio.h>

int test(int i, float j, bool k) {
	printf("One:%d Two:%f Three: %d\n", i, j, k);
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

	Menu menu;
	menu.InitWindow();

	JITCall jit((char*)&test);
	JITCall::tJitCall pCall = jit.getJitFunc("int", { "int", "float" , "bool"});

	JITCall::Parameters* params = (JITCall::Parameters*)(char*)new uint64_t[2];
	*(int*)params->getArgPtr(0) = 1;
	*(float*)params->getArgPtr(1) = 1337.0;
	*(bool*)params->getArgPtr(2) = true;
	pCall(params);
	getchar();
}

