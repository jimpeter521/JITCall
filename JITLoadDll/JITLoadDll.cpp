#include "JITCall.hpp"
#include "CommandParser.hpp"
#include "MainWindow.hpp"

#include <stdio.h>

int test(int i, float j) {
	printf("One:%d Two:%f\n", i, j);
	return 0;
}

// Represents a single jit'd stub & it's execution environment
struct JITEnv {
	JITEnv() {
		call = nullptr;
	}

	// holds jit runtime and builder
	std::unique_ptr<JITCall> jit;

	// holds jitted stub
	JITCall::tJitCall call;

	// holds params;
	std::unique_ptr<JITCall::Parameters> params;
};


/* WARNING: AsmJit MUST have ASMJIT_STATIC set and use /MT or /MTd for static linking
*  due to the fact that the source code is embedded. This is an artifact of our project structure
*/
int main()
{
	Command cmdParser(GetCommandLineA());
	std::vector<JITEnv> jitEnvs;

	std::string dllPath = "";
	HMODULE loadedModule = NULL;
	MainWindow window;
	window.OnFileChosen([&](std::string path) {
		std::cout << "File Chosen: " << path << std::endl;
		dllPath = path;

		// actally load
		loadedModule = LoadLibraryA(dllPath.c_str());
		return (uint64_t)loadedModule;
	});

	window.OnNewFunction([&](const std::vector<FunctionEditor::State::ParamState>& paramStates, const char* retType, const char* exportName, const char* callConv, bool insertBP) {
		uint64_t exportAddr = (uint64_t)((char*)GetProcAddress(loadedModule, exportName));
		std::cout << "Export: " << exportName << " " << std::hex<<  exportAddr  << std::dec << std::endl;
		
		JITEnv env;
		env.jit = std::make_unique<JITCall>((char*)exportAddr);
		env.params.reset(JITCall::Parameters::AllocParameters((uint8_t)paramStates.size()));

		// build param list of types from GUI state
		std::vector<std::string> paramTypes;
		for (uint8_t i = 0; i < paramStates.size(); i++) {
			auto pstate = paramStates.at(i);
			paramTypes.push_back(pstate.type);
			env.params->setArg<uint64_t>(i, pstate.getPacked());
		}

		env.call = env.jit->getJitFunc(retType, paramTypes, callConv, insertBP);	
		jitEnvs.push_back(std::move(env));
		std::cout << "Added a new JIT call" << std::endl;
	});

	window.InitWindow();

	// Invoke in order
	for(JITEnv& env : jitEnvs) {
		env.call(env.params.get());
	}
	
	getchar();
}

