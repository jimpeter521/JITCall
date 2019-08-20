#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include "FunctionEditor.hpp"

#include <imgui.h>
#include <examples/imgui_impl_win32.h>
#include <examples/imgui_impl_dx11.h>
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <stdint.h>
#include <functional>

class MainWindow;

namespace hack {
	static MainWindow* pWindow = nullptr;
}

class MainWindow {
public:
	typedef std::function<void(const std::vector<FunctionEditor::State::ParamState>& params, const char* retType)> tNewFunc;
	typedef std::function<void(std::string)> tFileChosen;

	MainWindow();
	bool InitWindow();
	
	LONG_PTR WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void OnNewFunction(tNewFunc callback) { this->callback = callback; }
	void OnFileChosen(tFileChosen callback) { this->fileCallback = callback;  }
private:
	FunctionEditor::State::ParamState getParamState(const uint8_t idx) const;
	const char* getReturnType() const;
	uint8_t getParamCount() const;

	void closeWindow() const;
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();

	HWND m_hwnd;
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pDeviceContext;
	IDXGISwapChain* m_pSwapChain;
	ID3D11RenderTargetView* m_pRenderTargetView;

	tNewFunc callback;
	tFileChosen fileCallback;
	bool m_showFilePicker;
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI ForwardWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!hack::pWindow)
		return 0;

	return hack::pWindow->WndProc(hWnd, msg, wParam, lParam);
}