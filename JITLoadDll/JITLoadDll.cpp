#include "JITCall.hpp"
#include "CmdParser/parser.hpp"

#include <string>
#include <stdio.h>

// Represents a single jit'd stub & it's execution environment
class JITEnv {
public:
	JITEnv(BoundFNTypeDef&& boundFn, const uint64_t exportAddr) : boundFn(std::move(boundFn)) {
		jit = std::make_unique<JITCall>((char*)exportAddr);
	}

	void invokeJit(const bool insertBP) {
		call = jit->getJitFunc(boundFn.typeDef.retType, boundFn.typeDef.argTypes, boundFn.typeDef.callConv, insertBP);
	}

	// holds jit runtime and builder (allocated runtime environment)
	std::unique_ptr<JITCall> jit;

	// holds jitted stub (final asm stub)
	JITCall::tJitCall call;

	// holds the function def + allocated parameters
	BoundFNTypeDef boundFn;
};

int main(int argc, char* argv[])
{
	// Parse cmd line into function objects
	auto cmdLine = parseCommandLine(argc, argv);
	if(!cmdLine) {
		std::cout << "error parsing commandline, exiting" << std::endl;
		return 1;
	}

	std::vector<JITEnv> jitEnvs;
	const bool insertBP = false;

	HMODULE loadedModule = LoadLibraryA(cmdLine->dllPath.c_str());
	for (uint8_t i = 0; i < cmdLine->exportFnMap.size(); i++) {
		std::string exportName = cmdLine->exportFnMap.at(i);
		uint64_t exportAddr = (uint64_t)((char*)GetProcAddress(loadedModule, exportName.c_str()));

		std::cout << "[-] Adding JIT Stub for Export: " << exportName << " at: " << std::hex << exportAddr << std::dec << " ..." << std::endl;

		JITEnv env(std::move(cmdLine->boundFunctions.at(i)), exportAddr);
		env.invokeJit(insertBP);

		jitEnvs.push_back(std::move(env));
		std::cout << "[+] Done." << std::endl;
	}
	
	// Invoke in order
	for(JITEnv& env : jitEnvs) {
		env.call(env.boundFn.params.get());
	}
	
	getchar();
	return 0;
}

