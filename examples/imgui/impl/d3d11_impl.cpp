#include "../../../kiero.h"

#if KIERO_INCLUDE_D3D11

#include "d3d11_impl.h"
#include <d3d11.h>
#include <assert.h>

#include "win32_impl.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "iostream"

typedef long(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
static Present oPresent = NULL;

ID3D11DeviceContext* context;
ID3D11RenderTargetView* g_mainRenderTargetView;
long __stdcall hkPresent11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	static bool init = false;

	if (!init)
	{
		DXGI_SWAP_CHAIN_DESC desc;
		pSwapChain->GetDesc(&desc);

		ID3D11Device* device;
		pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&device);

		device->GetImmediateContext(&context);

		ID3D11Texture2D* pBackBuffer;
		pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		device->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();

		impl::win32::init(desc.OutputWindow);

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(desc.OutputWindow);
		ImGui_ImplDX11_Init(device, context);

		init = true;
	}

	context->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	impl::MenuLoop();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return oPresent(pSwapChain, SyncInterval, Flags);
}

void impl::d3d11::init()
{
	kiero::bind(8, (void**)&oPresent, hkPresent11);
}

#endif // KIERO_INCLUDE_D3D11