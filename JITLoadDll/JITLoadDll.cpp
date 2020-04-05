#include "JITCall.hpp"
#include "CmdParser/parser.hpp"

#include <string>
#include <stdio.h>

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

int main(int argc, char* argv[])
{
	// Parse cmd line into function objects
	if (!parseFunctions(argc, argv))
		return 1;

	std::vector<JITEnv> jitEnvs;

	std::string dllPath = "blah_blah_blah";
	const char* exportName = "the export";

	const char* retType = "void";
	const char* callConv = "stdcall";
	const bool insertBP = false;

	HMODULE loadedModule = LoadLibraryA(dllPath.c_str());
	uint64_t exportAddr = (uint64_t)((char*)GetProcAddress(loadedModule, exportName));
	std::cout << "Export: " << exportName << " " << std::hex<<  exportAddr  << std::dec << std::endl;
		
    JITEnv env;
    env.jit = std::make_unique<JITCall>((char*)exportAddr);

    // build param list of types
	const uint8_t argCount = 4;
	env.params.reset(JITCall::Parameters::AllocParameters(0));
    std::vector<std::string> paramTypes;
    for (uint8_t i = 0; i < 0; i++) {
		std::string paramType = "int64_t";
		uint64_t paramVal = 0;

    	paramTypes.push_back(paramType);
		
    	env.params->setArg<uint64_t>(i, paramVal);
    }
    
    env.call = env.jit->getJitFunc(retType, paramTypes, callConv, insertBP);	
    jitEnvs.push_back(std::move(env));
    std::cout << "Added a new JIT call" << std::endl;

	// Invoke in order
	for(JITEnv& env : jitEnvs) {
		env.call(env.params.get());
	}
	
	getchar();
	return 0;
}

