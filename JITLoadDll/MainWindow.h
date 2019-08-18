#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include "MainContents.h"

#include <imgui.h>
#include <examples/imgui_impl_win32.h>
#include <examples/imgui_impl_dx11.h>
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <stdint.h>

class MainWindow;

namespace hack {
	static MainWindow* pWindow = nullptr;
}

class MainWindow {
public:
	MainWindow();
	bool InitWindow();
	
	LONG_PTR WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	MainContents::State::ParamState getParamState(const uint8_t idx) const;
	const char* getReturnType() const;
	uint8_t getParamCount() const;
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
	if (!hack::pWindow)
		return 0;

	return hack::pWindow->WndProc(hWnd, msg, wParam, lParam);
}