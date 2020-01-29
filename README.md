# JITCall
An olly inspired dll loader for x64dbg using JIT compiling instead of asm. Now you can call exports in x64dbg, without rundll32

![TypeDef Builder](https://i.imgur.com/Q5elpIC.png)

Test function, and JIT'd call stub output:
```
int test(int i, float j) {
	printf("One:%d Two:%f\n", i, j);
	return 0;
}
```

```
JIT Wrapper:
[RAPass::BuildCFG]
  L0: void Func(u64@rcx %0)
  {#0}
    mov %1, 0
    mov %2, qword [%0+%1]
    add %1, 8
    movq %3, qword [%0+%1]
    add %1, 8
    call 0x7FF68A628B43
    [FuncRet]
  {#1}
  L1:
    [FuncEnd]
[RAPass::BuildViews]
[RAPass::BuildDominators]
  IDom of #1 -> #0
  Done (2 iterations)
[RAPass::BuildLiveness]
  LiveIn/Out Done (4 visits)
  {#0}
    IN   [%0]
    GEN  [%1, %2, %0, %3]
    KILL [%1, %2, %3]
  {#1}
  %1 {id:0257 width: 8    freq: 0.6250 priority=0.6350}: [3:11]
  %2 {id:0258 width: 8    freq: 0.2500 priority=0.2600}: [5:13]
  %0 {id:0256 width: 7    freq: 0.2857 priority=0.2957}: [2:9]
  %3 {id:0259 width: 4    freq: 0.5000 priority=0.5100}: [9:13]
[RAPass::BinPack] Available=15 (0x0000FFEF) Count=3
  00: [3:11@257]
  01: [2:9@256]
  02: [5:13@258]
  Completed.
[RAPass::BinPack] Available=16 (0x0000FFFF) Count=1
  00: [9:13@259]
  Completed.
[RAPass::Rewrite]
.section .text {#0}
L0:
sub rsp, 0x28                               ; 4883EC28
mov rax, 0                                  ; 48C7C000000000          | <00002> mov %1, 0                        | %1{W|Out}
mov edx, qword [rcx+rax]                    ; 8B1401                  | <00004> mov %2, qword [%0+%1]            | %2{W|Out} %0{R|Use} %1{R|Use}
add rax, 8                                  ; 4883C008                | <00006> add %1, 8                        | %1{X|Use}
movq xmm0, qword [rcx+rax]                  ; F30F7E0401              | <00008> movq %3, qword [%0+%1]           | %0{R|Use|Last|Kill} %1{R|Use} %3{W|Out}
add rax, 8                                  ; 4883C008                | <00010> add %1, 8                        | %1{X|Use|Last|Kill}
mov ecx, edx                                ; 8BCA                    | <MOVE> %2
movdqa xmm1, xmm0                           ; 660F6FC8                | <MOVE> %3
call 0x7FF68A628B43                         ; 40E800000000            | <00012> call 0x7FF68A628B43              | %2{R|Use=1|Last|Kill} %3{R|Use=1|Last|Kill}
L1:                                         ;                         | L1:
add rsp, 0x28                               ; 4883C428
ret                                         ; C3
 
One:1337 Two:1338.000000
```

# Setup

```
git clone --recursive https://github.com/stevemk14ebr/JITCall.git
cd JITCall
git submodule update --init --recursive
```

# Implementation (JIT works, need to stitch command line stuff in still)
Stole x64dbgs command parser to preserve style, parse command line with that. Use arguments from command line to fill a uint64_t parameter array, utilizing type puning to just shove raw bytes in. Abuse ASMJit to JIT a little wrapper function that will take the parameter array as input and map the slots in the array to the correct ABI locations (stack/reg) for the call we are doing. Then invoke this JIT stub from C.

Because we just shove all params into a uint64_t array and push the actual call to runtime JIT we don't need to hand craft any fancy assembly, just need to provide the correct type def at runtime so that asmjit knows the ABI to map to. 
