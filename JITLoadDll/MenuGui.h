#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <imgui.h>
#include <examples/imgui_impl_win32.h>
#include <examples/imgui_impl_dx11.h>
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <stdint.h>

class Menu;

namespace hack {
	static Menu* pMenu = nullptr;
}

class Menu {
public:
	Menu();
	bool InitWindow();

	LONG_PTR WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();

	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pDeviceContext;
	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pRenderTargetView;
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI ForwardWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!hack::pMenu)
		return 0;

	return hack::pMenu->WndProc(hWnd, msg, wParam, lParam);
}
