#include "JITCall.hpp"
#include "clipp.h"

#include <map>
#include <vector>
#include <regex>
#include <sstream>
#include <optional>
#include <string>

#include <stdio.h>
#include <shellapi.h>

std::string u16Tou8(const std::wstring s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	std::string r(len, '\0');
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
	return r;
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
std::vector<std::string> supportedCallConvs { "stdcall", "cdecl", "fastcall"};
std::vector<std::string> supportedTypes { "void", "int8_t", "uint8_t", "int16_t", "uint16_t",
	"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double",
	"char*"
};

struct DataTypeInfo {
	uint8_t size;
	std::string formatStr;
};

std::map<std::string, DataTypeInfo> typeFormats {
	{"void", {0, ""}},
	{"int8_t", {1, "%d"}},
	{"uint8_t", {1, "%u"}},
	{"int16_t", {sizeof(int16_t), "%d"}},
	{"uint16_t", {sizeof(uint16_t), "%u"}},
	{"int32_t", {sizeof(int32_t), "%d"}},
	{"uint32_t", {sizeof(uint32_t), "%u"}},
#ifdef _MSC_VER
	{"int64_t", {sizeof(int64_t), "%I64d"}},
	{"uint64_t", {sizeof(uint64_t), "%I64u"}},
#else
	{"int64_t", {sizeof(int64_t), "%lld"}},
	{"uint64_t", {sizeof(uint64_t), "%llu"}},
#endif
	{"float", {sizeof(float), "%f"}},
	{"double", {sizeof(double), "%lf"}}
};

bool formatType(std::string type, std::string data, char* outData) {
	char buf[64];
	DataTypeInfo typeInfo = typeFormats.at(type);
	return sscanf_s(data.c_str(), typeInfo.formatStr.c_str(), outData) == 1;
}

void printUsage(clipp::group& cli, char* argv[]) {
	std::cout << "Supported calling conventions: " << std::endl;
	for (auto& conv : supportedCallConvs)
		std::cout << conv << std::endl;

	std::cout << "Supported types: " << std::endl;
	for (auto& t : supportedTypes)
		std::cout << t << std::endl;

	std::cout << clipp::make_man_page(cli, argv[0]) << std::endl;
}

int main(int argc, char* argv[])
{
	std::vector<std::string> cmdFnTypeDef;
	std::vector<std::vector<std::string>> cmdFnArgs;

	// default initialize these elems
	const uint8_t fnMaxCount = 5;
	cmdFnArgs.resize(fnMaxCount); 
	cmdFnTypeDef.resize(fnMaxCount);

	auto cli = clipp::group{};
	for (uint8_t i = 0; i < fnMaxCount; i++) {
		cli.push_back(std::move(
			clipp::option("-f" + std::to_string(i + 1), "--func" + std::to_string(i + 1)) & clipp::value("typedef", cmdFnTypeDef[i]) & clipp::values("args", cmdFnArgs[i])
		));
	}

	if (parse(argc, argv, cli)) {
		for (uint8_t i = 0; i < cmdFnTypeDef.size(); i++) {
			std::string typeDef = cmdFnTypeDef[i];
			std::vector<std::string> args = cmdFnArgs[i];
			if (!typeDef.size())
				break;

			if (auto fnDef = regex_typedef(typeDef)) {
				if (fnDef->argTypes.size() != args.size()) {
					std::cout << "invalid parameter count supplied to function: " + std::to_string(i) << " exiting" << std::endl;
					return 1;
				}

				std::cout << typeDef << " ";
				for (auto arg : args) {
					std::cout << arg << " ";
				}
				std::cout << std::endl;
			} else {
				std::cout << "invalid function typedef provided, exiting" << std::endl;
				return 1;
			}
		}
	} else {
		printUsage(cli, argv);
		return 1;
	}

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

