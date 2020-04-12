#pragma once

#include "clipp.h"
#include "JITCall.hpp"

#include <map>
#include <vector>
#include <regex>
#include <sstream>
#include <optional>
#include <string>
#include <iostream>
#include <algorithm> 
#include <cctype>
#include <locale>

#include <stdio.h>
#include <stringapiset.h>

/***
*	This file is responsible for parsing the command line into objects that are JIT-able. 
*   A JIT-able object only requires the complete function typedef and the argument byte to be passed.
*
*	The commandline format is a function typedef with X number of args specified as strings.
*   The parsing will take the typedef into accounts, interpret that argument strings via the types in the typedefs,
*   and then alloc memory and read the args into argument buffers according to their true types. Ex:
*   -f void(float a) "0.1337" is read into a byte buffer so that it's bytes are the same as float a = 0.1337;
*   It's also responsible for verifying arg counts match the typedef given counts, and that types are valid.
*   
*   TODO: In the future it will be responsible for reading files into byte buffers and opening handles and such.
***/

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

static inline std::string ltrim_copy(std::string s) {
	ltrim(s);
	return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
	rtrim(s);
	return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
	trim(s);
	return s;
}

std::vector<std::string> split(const std::string s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> elems;
	while (std::getline(ss, item, delim)) {
		elems.push_back(std::move(item));
	}
	return elems;
}

std::string u16Tou8(const std::wstring s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	std::string r(len, '\0');
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
	return r;
}

struct FNTypeDef {
	FNTypeDef() {
		argTypes = std::vector<std::string>();
	}

	std::string retType;
	std::string callConv;
	std::vector<std::string> argTypes;
};


// A FNTypeDef w/ associated parameters
struct BoundFNTypeDef {
	BoundFNTypeDef() = delete;

	BoundFNTypeDef(const uint8_t numArgs) : 
		params(JITCall::Parameters::AllocParameters(numArgs)) {}

	FNTypeDef typeDef;

	// holds param values
	std::unique_ptr<JITCall::Parameters> params;
};

std::optional<FNTypeDef> regex_typedef(std::string input) {
	//"([a-zA-Z_][a-zA-Z0-9_*]*)\s*(?:([a-zA-Z_][a-zA-Z0-9_*]*)?\s*)?(?:[a-zA-Z_][a-zA-Z0-9_]*)?\s*\((.*)\)" 
    //ex: void (int a, int*,int64_t ,int *b, char) or void stdcall (int a, int*,int64_t ,int *b, char)
	std::regex fnTypeDefRgx("([a-zA-Z_][a-zA-Z0-9_*]*)\\s*(?:([a-zA-Z_][a-zA-Z0-9_*]*)?\\s*)?(?:[a-zA-Z_][a-zA-Z0-9_]*)?\\s*\\((.*)\\)");
	                         
	std::smatch matches;

	if (std::regex_search(input, matches, fnTypeDefRgx)) {
		FNTypeDef functionDefinition;
		functionDefinition.retType = trim_copy(matches[1].str());
		functionDefinition.callConv = trim_copy(matches[2].str());

		// if none specified, use fastcall for x64 or cdecl for x86. These are ABI defined defaults
		if (functionDefinition.callConv.length() == 0) {
			functionDefinition.callConv = sizeof(char*) == 4 ? "cdecl" : "fastcall";
		}
		auto params = matches[3].str();

		// split the arguments, they are all captured as one big group
		for (std::string argStr : split(params, ',')) {
			std::regex fnArgRgx("([a-zA-Z_][a-zA-Z0-9_*]*)");

			if (std::regex_search(argStr, fnArgRgx)) {
				// trim off variable names by space, take care if * is attached to name
				std::string argTrimmed = trim_copy(argStr);
				auto space_idx = argTrimmed.find_first_of(' ');
				if (space_idx == std::string::npos) {
					functionDefinition.argTypes.push_back(argTrimmed);
				}
				else {
					// whoops we lost the * by trimming by space, add it back.
					std::string argType = argTrimmed.substr(0, space_idx);
					if (argTrimmed.find("*") != std::string::npos && argType.find("*") == std::string::npos) {
						argType += "*";
					}
					functionDefinition.argTypes.push_back(argType);
				}
			}
			else {
				std::cout << "Invalid function argument definition: " << argStr << std::endl;
				return {};
			}
		}
		return functionDefinition;
	}
	else {
		std::cout << "Invalid function definition given: " << input << std::endl;
		return {};
	}
}

/*
*  WARNING: AsmJit MUST have ASMJIT_STATIC set and use /MT or /MTd for static linking
*  due to the fact that the source code is embedded. This is an artifact of our project structure
*/
std::vector<std::string> supportedCallConvs{ "stdcall", "cdecl", "fastcall" };
std::vector<std::string> supportedTypes{ "void", "int8_t", "uint8_t", "int16_t", "uint16_t",
	"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double",
	"char*"
};

struct DataTypeInfo {
	uint8_t size;
	std::string formatStr;
};

std::map<std::string, DataTypeInfo> typeFormats{
	{"void", {0, ""}},
	{"int8_t", {1, "%d"}},
	{"char", {1, "%d"}},
	{"uint8_t", {1, "%u"}},
	{"int16_t", {sizeof(int16_t), "%d"}},
	{"uint16_t", {sizeof(uint16_t), "%u"}},
	{"int32_t",  {sizeof(int32_t), "%d"}},
	{"int",  {sizeof(int32_t), "%d"}},
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

bool formatType(std::string type, std::string data, uint64_t* outData) {
	memset(outData, 0, sizeof(uint64_t));

	// if any type is ptr, make it the arch's int ptr type
	if (type.find("*") != std::string::npos) {
		type = sizeof(char*) == 4 ? "uint32_t" : "uint64_t";
	}

	if (typeFormats.count(type) == 0)
		return false;

	DataTypeInfo typeInfo = typeFormats.at(type);
	bool success = sscanf_s(data.c_str(), typeInfo.formatStr.c_str(), (char*)outData) == 1;
	if (!success)
		return false;

	return true;
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

struct CommandLineInput {
	std::vector<BoundFNTypeDef> boundFunctions;

	// boundFunction idx -> export name
	std::map<uint8_t, std::string> exportFnMap; 
	std::string dllPath;
};

std::optional<CommandLineInput> parseCommandLine(int argc, char* argv[]) {
	CommandLineInput commandlineInput;

	std::string cmdInputFile;
	std::vector<std::string> cmdFnExport;
	std::vector<std::string> cmdFnTypeDef;
	std::vector<std::vector<std::string>> cmdFnArgs;
	
	// default initialize these elems
	const uint8_t fnMaxCount = 5;
	cmdFnArgs.resize(fnMaxCount);
	cmdFnTypeDef.resize(fnMaxCount);
	cmdFnExport.resize(fnMaxCount);

	// accepts a list of typedes then their arguments
	auto cli = clipp::group{};
	cli.push_back(clipp::option("-i") & clipp::value("inputFilePath", cmdInputFile));

	for (uint8_t i = 0; i < fnMaxCount; i++) {
		cli.push_back(std::move(
			clipp::option("-f" + std::to_string(i + 1), "--func" + std::to_string(i + 1)) & clipp::value("export", cmdFnExport[i]) & clipp::value("typedef", cmdFnTypeDef[i]) & clipp::values("args", cmdFnArgs[i])
		));
	}

	if (parse(argc, argv, cli)) {
		commandlineInput.dllPath = cmdInputFile;

		// for each typedef
		for (uint8_t i = 0; i < cmdFnTypeDef.size(); i++) {
			std::string typeDef = cmdFnTypeDef[i];
			std::vector<std::string> args = cmdFnArgs[i];
			if (!typeDef.size())
				break;

			// parse the typedef arg types via regex
			if (auto fnDef = regex_typedef(typeDef)) {
				if (fnDef->argTypes.size() != args.size()) {
					std::cout << "invalid parameter count supplied to function: " + std::to_string(i) << " exiting" << std::endl;
					return {};
				}

				std::cout << typeDef << " ";
				// for each of the argument types, reinterpret the string data for that type
				BoundFNTypeDef jitTypeDef((uint8_t)fnDef->argTypes.size());
				jitTypeDef.typeDef = *fnDef;

				for (uint8_t j = 0; j < fnDef->argTypes.size(); j++) {
					std::string argType = fnDef->argTypes[j];
					std::cout << argType << " " << args[j];

					// pack all types into a uint64_t
					uint64_t argBuf;
					if (!formatType(argType, args[j], &argBuf)) {
						std::cout << "failed to parse argument with given type" << std::endl;
						return {};
					}

					jitTypeDef.params->setArg(j, argBuf);
				}

				commandlineInput.boundFunctions.push_back(std::move(jitTypeDef));
				commandlineInput.exportFnMap[i] = cmdFnExport[i];
				std::cout << std::endl;
			}
			else {
				std::cout << "invalid function typedef provided, exiting" << std::endl;
			}
		}
	} else {
		printUsage(cli, argv);
	}

	return commandlineInput;
}