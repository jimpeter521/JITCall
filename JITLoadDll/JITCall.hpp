#pragma once

#include "ErrorLog.hpp"

#define ASMJIT_EMBED 
#include <asmjit/asmjit.h>

#include <stdint.h>
#include <string>
#include <vector>

#if defined(__clang__)

#elif defined(__GNUC__) || defined(__GNUG__)
#define NOINLINE __attribute__((noinline))
#define PH_ATTR_NAKED __attribute__((naked))
#define OPTS_OFF _Pragma("GCC push_options") \
_Pragma("GCC optimize (\"O0\")")
#define OPTS_ON #pragma GCC pop_options
#elif defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#define JC_ATTR_NAKED __declspec(naked)
#define OPTS_OFF __pragma(optimize("", off))
#define OPTS_ON __pragma(optimize("", on))
#endif

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
		char* getArgPtr(const uint8_t idx) const {
			return (char*)&m_arguments[idx];
		}

		// asm depends on this specific type
		volatile uint64_t m_arguments[1];

		/*
		* Flexible array members like above are not valid in C++ and are U.B. However, we make
		* sure that we allocate the actual memory we touch when we access beyond index [0]. However,
		* this is STILL not enough as the compiler is allowed to optmize away U.B. so we make one
		* additional attempt to always access through a char* (must be char*, not unsigned char*) to avoid aliasing rules which helps
		* the compiler not be ridiculous. It's still NOT safe, but it's good enough 99.99% of the time.
		* Oh and volatile might help this too, so we add that.
		*/
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