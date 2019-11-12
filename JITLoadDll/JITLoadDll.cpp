#include "JITCall.hpp"
#include "CommandParser.hpp"
#include "MainWindow.hpp"
#include "PEB.hpp"

#include <stdio.h>
#include <string>
#include <vector>
#include <cassert>

int test(int i, float j) {
	printf("One:%d Two:%f\n", i, j);
	return 0;
}

// Represents a single jit'd stub & it's execution environment
struct JITEnv {
	// holds jit runtime and builder
	std::unique_ptr<JITCall> jit;

	// holds jitted stub
	JITCall::tJitCall call;

	// holds params;
	std::unique_ptr<JITCall::Parameters> params;
};

std::vector<std::string> getExports(uint64_t moduleBase) {
	assert(moduleBase != NULL);
	if (moduleBase == NULL)
		return std::vector<std::string>();

	IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)moduleBase;
	IMAGE_NT_HEADERS* pNT = RVA2VA(IMAGE_NT_HEADERS*, moduleBase, pDos->e_lfanew);
	IMAGE_DATA_DIRECTORY* pDataDir = (IMAGE_DATA_DIRECTORY*)pNT->OptionalHeader.DataDirectory;

	if (pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == NULL) {
		ErrorLog::singleton().push("PEs without export tables are unsupported", ErrorLevel::SEV);
		return std::vector<std::string>();
	}

	IMAGE_EXPORT_DIRECTORY* pExports = RVA2VA(IMAGE_EXPORT_DIRECTORY*, moduleBase, pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	uint32_t* pAddressOfFunctions = RVA2VA(uint32_t*, moduleBase, pExports->AddressOfFunctions);
	uint32_t* pAddressOfNames = RVA2VA(uint32_t*, moduleBase, pExports->AddressOfNames);
	uint16_t* pAddressOfNameOrdinals = RVA2VA(uint16_t*, moduleBase, pExports->AddressOfNameOrdinals);

	std::vector<std::string> exports;
	exports.reserve(pExports->NumberOfNames);
	for (uint32_t i = 0; i < pExports->NumberOfNames; i++)
	{
		char* exportName = RVA2VA(char*, moduleBase, pAddressOfNames[i]);
		exports.push_back(exportName);
	}
	return exports;
}

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

		std::vector<std::string> exports = getExports((uint64_t)loadedModule);
		std::cout << "--Export Table Start--" << std::endl;
		for (auto&& exp : exports) {
			std::cout << exp << std::endl;
		}
		std::cout << "--Export Table End--" << std::endl;
	});

	window.OnNewFunction([&](const std::vector<FunctionEditor::State::ParamState>& paramStates, const char* retType, const char* exportName) {
		uint64_t exportAddr = (uint64_t)((char*)GetProcAddress(loadedModule, exportName));
		std::cout << "Export: " << exportName << " " << std::hex<<  exportAddr  << std::dec << std::endl;
		
		JITEnv env;
		env.jit = std::make_unique<JITCall>((char*)exportAddr);
		env.params.reset((JITCall::Parameters*)new uint64_t[paramStates.size()]);

		memset(env.params.get(), 0, sizeof(uint64_t) * paramStates.size());

		// build param list of types from GUI state
		std::vector<std::string> paramTypes;
		for (uint8_t i = 0; i < paramStates.size(); i++) {
			auto pstate = paramStates.at(i);
			paramTypes.push_back(pstate.type);
			*(uint64_t*)env.params->getArgPtr(i) = pstate.getPacked();
		}

		env.call = env.jit->getJitFunc(retType, paramTypes);	
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

