#pragma once

#include "clipp.h"

#include <map>
#include <vector>
#include <regex>
#include <sstream>
#include <optional>
#include <string>
#include <iostream>

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
	if (typeFormats.count(type) == 0)
		return false;

	DataTypeInfo typeInfo = typeFormats.at(type);
	bool success = sscanf_s(data.c_str(), typeInfo.formatStr.c_str(), &buf[0]) == 1;
	if (!success)
		return false;

	memset(outData, 0, 64);
	memcpy(outData, buf, typeInfo.size);
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

bool parseFunctions(int argc, char* argv[]) {
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
			}
			else {
				std::cout << "invalid function typedef provided, exiting" << std::endl;
				return false;
			}
		}
	} else {
		printUsage(cli, argv);
		return false;
	}

	return true;
}