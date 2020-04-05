#include "JITCall.hpp"

#include <regex>
#include <stdio.h>
#include <sstream>
#include <optional>

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

struct FNTypeDef {
	FNTypeDef() {
		argTypes = std::vector<std::string>();
	}

	std::string retType;
	std::vector<std::string> argTypes;
};

std::vector<std::string> split(const std::string s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> elems;
	while (std::getline(ss, item, delim)) {
		elems.push_back(std::move(item));
	}
	return elems;
}

std::optional<FNTypeDef> regex_typedef(std::string input) {
	//"([a-zA-Z_][a-zA-Z0-9_*]*)\s*(?:[a-zA-Z_][a-zA-Z0-9_]*)?\s*\((.*)\)" ex: void (int a, int*,int64_t ,int *b, char)
	std::regex fnTypeDefRgx("([a-zA-Z_][a-zA-Z0-9_*]*)\\s*(?:[a-zA-Z_][a-zA-Z0-9_]*)?\\s*\\((.*)\\)");
	std::smatch matches;

	if (std::regex_search(input, matches, fnTypeDefRgx)) {
		FNTypeDef functionDefinition;
		functionDefinition.retType = matches[1].str();

		auto params = matches[2].str();

		for (std::string argStr : split(params, ',')) {
			std::regex fnArgRgx("([a-zA-Z_][a-zA-Z0-9_*]*)");
			
			if (std::regex_search(argStr, fnArgRgx)) {
				functionDefinition.argTypes.push_back(argStr);
			} else {
				std::cout << "Invalid function argument definition: " << argStr << std::endl;
				return {};
			}
		}
		return functionDefinition;
	} else {
		std::cout << "Invalid function definition given: "<< input << std::endl;
		return {};
	}
}

/* 
*  WARNING: AsmJit MUST have ASMJIT_STATIC set and use /MT or /MTd for static linking
*  due to the fact that the source code is embedded. This is an artifact of our project structure
*/
int main()
{
	bool is_fn = regex_typedef("void (int a, int*,int64_t ,int *b, char)").has_value();

	char* cmdLine = GetCommandLineA();
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
	env.params.reset(JITCall::Parameters::AllocParameters(argCount));
    std::vector<std::string> paramTypes;
    for (uint8_t i = 0; i < argCount; i++) {
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
}

