#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>

extern "C" __declspec(dllexport) void exportOneCdecl(int i) {
	printf("Hello from export one: %d", i);
}

extern "C" __declspec(dllexport) void __stdcall exportOneCdecl(int i) {
    printf("Hello from export one stdcall: %d", i);
}

extern "C" __declspec(dllexport) void exportTwoCdecl(float i) {
    printf("Hello from export two: %f", i);
}

extern "C" __declspec(dllexport) void __stdcall exportTwo(float i) {
	printf("Hello from export two: %f", i);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
