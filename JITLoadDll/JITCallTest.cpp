#include "JITCall.hpp"
#include "catch.hpp"

NOINLINE void __cdecl noArgs() {
	volatile int i = 10;
	std::string blah = "nothing to see here";
	printf("basic: %d %s", i, blah.c_str());
}

TEST_CASE("Test x86 function JIT", "[JITCall]") {
	SECTION("No parameters") {
		JITCall jit((uint64_t)&noArgs);
		
		JITCall::tJitCall jitFunc = jit.getJitFunc("void", {}, "cdecl");

		JITCall::Parameters params;
		jitFunc(&params);
		REQUIRE(true);
	}
}