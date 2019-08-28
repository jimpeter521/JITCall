#pragma once

#include "ErrorLog.hpp"

#define ASMJIT_EMBED 
#include <asmjit/asmjit.h>

#include <stdint.h>
#include <string>
#include <vector>

class SimpleErrorHandler : public asmjit::ErrorHandler {
public:
	inline SimpleErrorHandler() : err(asmjit::kErrorOk) {}

	void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
		this->err = err;
		fprintf(stderr, "ERROR: %s\n", message);
	}

	asmjit::Error err;
};

class JITCall {
public:
	struct Parameters {
		// must be char* for aliasing rules to work when reading back out
		unsigned char* getArgPtr(const uint8_t idx) const {
			return (unsigned char*)&m_arguments[idx];
		}

		// asm depends on this specific type
		uint64_t m_arguments[];
	};

	typedef void(*tJitCall)(Parameters* params);

	JITCall(uint64_t target);
	JITCall(char* target);
	~JITCall();

	/* Construct a callback given the raw signature at runtime.*/
	tJitCall getJitFunc(const std::string& retType, const std::vector<std::string>& paramTypes, std::string callConv = "");
private:
	tJitCall getJitFunc(const asmjit::FuncSignature& sig);

	// does a given type fit in a general purpose register (i.e. is it integer type)
	bool isGeneralReg(const uint8_t typeId) const;

	// float, double, simd128
	bool isXmmReg(const uint8_t typeId) const;

	// call conv string to asmjit type
	asmjit::CallConv::Id getCallConv(const std::string& conv);

	// std C type to asmjit type
	uint8_t getTypeId(const std::string& type);

	asmjit::JitRuntime m_jitRuntime;
	uint64_t m_Target;
};