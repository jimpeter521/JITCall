#include "MainWindow.hpp"
#include "imgui_filedialog.hpp"

MainWindow::MainWindow() {
	m_pDevice = nullptr;
	m_pDeviceContext = nullptr;
	m_pSwapChain = nullptr;
	m_pRenderTargetView = nullptr;

	hack::pWindow = this;
	m_showFilePicker = true;
	m_windowSize = { 600, 400 };

	m_hwnd = 0;
}

bool MainWindow::InitWindow() {
	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, ForwardWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("JITLoadDll"), NULL };
	::RegisterClassEx(&wc);

	m_hwnd = ::CreateWindow(wc.lpszClassName, _T("JITLoadDll"), WS_OVERLAPPEDWINDOW, 100, 100, (int)m_windowSize.x, (int)m_windowSize.y, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	if (!CreateDeviceD3D(m_hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(m_hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(m_hwnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

	// Our state
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.f,0.f,0.f, 1.00f);

	// Initially null, fileOpenCallback sets this
	uint64_t dllBase = NULL;

	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

		if (FunctionEditor::state.finished) {
			callback(FunctionEditor::state.params, FunctionEditor::state.returnType, FunctionEditor::state.exportName.data());
			FunctionEditor::state = FunctionEditor::State(dllBase); // mhm that's sexy, gotta love immediate mode
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(m_windowSize);
		
		ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		{
			ImGui::Begin("Choose Export", NULL, winFlags);
			if (m_showFilePicker) {
				auto filePicker = ImGuiFileDialog::Instance();
				filePicker->setPos(ImVec2(0, 0));
				filePicker->setSize(m_windowSize);
				if (filePicker->FileDialog("Choose File", ".dll\0\0", ".", "")) {
					if (filePicker->IsOk) {
						dllBase = fileCallback(ImGuiFileDialog::Instance()->GetFilepathName());

						// Re-init state now that DLL loaded
						FunctionEditor::state = FunctionEditor::State(dllBase); 
						m_showFilePicker = false;
					} else {
						ImGui::OpenPopup("FilePickError");
					}
				}

				if (ImGui::BeginPopup("FilePickError"))
				{
					ImGui::TextColored(ImColor::HSV(356.09f, 68.05f, 66.27f), "%s", "You must select a file to load");
					ImGui::EndPopup();
				}
			} else {
				FunctionEditor::Draw();
			}
			ImGui::End();
		}

		// Rendering
		ImGui::Render();
		m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, (float*)& clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present with vsync
		m_pSwapChain->Present(1, 0); 
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(m_hwnd);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);

	return 0;
}

LONG_PTR WINAPI MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (m_pDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
		    m_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			m_windowSize.x = LOWORD(lParam);
			m_windowSize.y = HIWORD(lParam);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		// Kill Window
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

bool MainWindow::CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pDevice, &featureLevel, &m_pDeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void MainWindow::CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (m_pSwapChain) { 
		m_pSwapChain->Release(); 
		m_pSwapChain = NULL; 
	}
	
	if (m_pDeviceContext) { 
		m_pDeviceContext->Release(); 
		m_pDeviceContext = NULL; 
	}
	
	if (m_pDevice) { 
		m_pDevice->Release(); 
		m_pDevice = NULL; 
	}
}

void MainWindow::CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	HRESULT result = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (SUCCEEDED(result)) {
		m_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_pRenderTargetView);
		pBackBuffer->Release();
	}
}

void MainWindow::CleanupRenderTarget()
{
	if (m_pRenderTargetView) { 
		m_pRenderTargetView->Release(); 
		m_pRenderTargetView = NULL; 
	}
}

FunctionEditor::State::ParamState MainWindow::getParamState(const uint8_t idx) const
{
	if (idx >= FunctionEditor::state.params.size())
		return FunctionEditor::State::ParamState();

	return FunctionEditor::state.params.at(idx);
}

uint8_t MainWindow::getParamCount() const {
	return (uint8_t)FunctionEditor::state.params.size();
}

const char* MainWindow::getReturnType() const {
	return FunctionEditor::state.returnType;
}

void MainWindow::closeWindow() const {
	SendMessage(m_hwnd, WM_DESTROY, 0, 0);
}