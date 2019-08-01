# JITCall
An olly inspired dll loader for x64dbg using JIT compiling instead of asm. Now you can call exports in x64dbg, without rundll32

# Implementation (JIT works, need to stitch command line stuff in still)
Stole x64dbgs command parser to preserve style, parse command line with that. Use arguments from command line to fill a uint64_t parameter array, utilizing type puning to just shove raw bytes in. Abuse ASMJit to JIT a little wrapper function that will take the parameter array as input and map the slots in the array to the correct ABI locations (stack/reg) for the call we are doing. Then invoke this JIT stub from C.

Because we just shove all params into a uint64_t array and push the actual call to runtime JIT we don't need to hand craft any fancy assembly, just need to provide the correct type def at runtime so that asmjit knows the ABI to map to. 
