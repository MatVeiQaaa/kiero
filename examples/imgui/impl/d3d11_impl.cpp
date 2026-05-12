#include "../../../kiero.h"

#if KIERO_INCLUDE_D3D11

#include "d3d11_impl.h"
#include <d3d11.h>
#include <assert.h>
#include <iostream>

#include "win32_impl.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"

#include "Helpers/Helpers.hpp"
#include "ImGuiInjector/ImGuiInjector.hpp"

typedef long(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
static Present oPresent = NULL;

long __stdcall hkPresent11(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	static bool init = false;
	static HWND TargetHwnd = 0;
	static ID3D11Device* device = 0;
	static ID3D11DeviceContext* context = 0;

	if (!init)
	{
		DXGI_SWAP_CHAIN_DESC desc;
		pSwapChain->GetDesc(&desc);

		pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&device);
		TargetHwnd = desc.OutputWindow;
		assert(TargetHwnd != NULL && "No window handle in hkPresent11()");

		device->GetImmediateContext(&context);

		ID3D11Texture2D* pBackBuffer;
		pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		pBackBuffer->Release();

		impl::win32::init(TargetHwnd);

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(TargetHwnd);
		ImGui_ImplDX11_Init(device, context);

		ImGuiViewport* vp = ImGui::GetMainViewport();
		assert(vp != NULL && "ImGuiViewport was nullptr in hkPresent11()");
		vp->PlatformHandleRaw = reinterpret_cast<void*>(TargetHwnd);

		ImGuiInjector::Get().LoadJapaneseFont();
		ImGuiInjector::Get().SetStartingStyle(ImGui::GetStyle());

		init = true;
	}

	{
		DXGI_SWAP_CHAIN_DESC desc;
		pSwapChain->GetDesc(&desc);
		ID3D11Device* nowDevice;
		pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&nowDevice);

		if (TargetHwnd != desc.OutputWindow) {
			TargetHwnd = desc.OutputWindow;
			ImGui_ImplWin32_Shutdown();
			ImGui_ImplWin32_Init(TargetHwnd);
		}
		if (device != nowDevice) {
			context->Release();
			device->Release();
			device = nowDevice;
			nowDevice->AddRef();
			device->GetImmediateContext(&context);
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplDX11_Init(device, context);
		}
		nowDevice->Release();
	}

	ImGuiInjector::Get().UpdateGlobalScale();
	ImGuiInjector::Get().ResetInput();
	if (!ImGuiInjector::Get().IsMenuRunning()) return oPresent(pSwapChain, SyncInterval, Flags);

	ID3D11DeviceContext* nowContext = 0;
	device->GetImmediateContext(&nowContext);
	if (context != nowContext) {
		std::cout << "Huh!" << '\n';
	}
	nowContext->Release();
	ID3D11Texture2D* backbuffer;
	ID3D11RenderTargetView* targetView;
	D3D11_TEXTURE2D_DESC desc{};
	pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
	device->CreateRenderTargetView(backbuffer, NULL, &targetView);
	backbuffer->GetDesc(&desc);
	backbuffer->Release();
	context->OMSetRenderTargets(1, &targetView, NULL);
	targetView->Release();
	ImVec2 canvasSize(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
	ImGuiInjector::Get().SetCanvasSize(canvasSize);
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	impl::MenuLoop();

	ImGui::EndFrame();
	ImGui::Render();
	// ImGui doesn't behave correctly when window and canvas size differ. 
	// https://github.com/ocornut/imgui/issues/6955
	ImDrawData* draw_data = ImGui::GetDrawData();
	ImVec2 scale;
	scale.x = canvasSize.x / ImGui::GetMainViewport()->Size.x;
	scale.y = canvasSize.y / ImGui::GetMainViewport()->Size.y;
	if (draw_data) {
		for (auto& list : draw_data->CmdLists) {
			for (auto& vert : list->VtxBuffer) {
				vert.pos.x *= scale.x;
				vert.pos.y *= scale.y;
			}
		}
		draw_data->DisplaySize.x = canvasSize.x;
		draw_data->DisplaySize.y = canvasSize.y;
		draw_data->ScaleClipRects(scale);
	}
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	ImGuiInjector::Get().UpdateInput();

	return oPresent(pSwapChain, SyncInterval, Flags);
}

void impl::d3d11::init()
{
	kiero::bind(8, (void**)&oPresent, hkPresent11);
}

#endif // KIERO_INCLUDE_D3D11