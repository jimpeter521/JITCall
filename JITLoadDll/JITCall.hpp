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
	// Do not modify this structure at all. There's alot of nuance
	struct Parameters {
		static Parameters* AllocParameters(const uint8_t numArgs) {
			auto params = (Parameters*)new uint64_t[numArgs];
			memset(params, 0, sizeof(uint64_t) * numArgs);
			return params;
		}

		template<typename T>
		void setArg(const uint8_t idx, const T val) {
			*(T*)getArgPtr(idx) = val;
		}

		template<typename T>
		T getArg(const uint8_t idx) const {
			return *(T*)getArgPtr(idx);
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
	private:

		// must be char* for aliasing rules to work when reading back out
		char* getArgPtr(const uint8_t idx) {
			return (char*)&m_arguments[idx];
		}
	};

	typedef void(*tJitCall)(Parameters* params);

	JITCall(uint64_t target);
	JITCall(char* target);
	~JITCall();

	/* Construct a callback given the raw signature at runtime. Optionally insert a breakpoint before the call*/
	tJitCall getJitFunc(const std::string& retType, const std::vector<std::string>& paramTypes, std::string callConv = "", bool breakpoint = false);
private:
	// given a signature, JIT a call stub, optionally inserting a breakpoint before the call
	tJitCall getJitFunc(const asmjit::FuncSignature& sig, bool breakpoint);

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