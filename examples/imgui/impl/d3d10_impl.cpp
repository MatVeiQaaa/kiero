#include "../../../kiero.h"

#if KIERO_INCLUDE_D3D10

#include "d3d10_impl.h"
#include <d3d10.h>
#include <assert.h>

#include "win32_impl.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx10.h"

typedef long(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
static Present oPresent = NULL;

long __stdcall hkPresent10(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	static bool init = false;

	ID3D10Device* pDevice;
	pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&pDevice);

	if (!init)
	{
		DXGI_SWAP_CHAIN_DESC desc;
		pSwapChain->GetDesc(&desc);

		impl::win32::init(desc.OutputWindow);

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(desc.OutputWindow);
		ImGui_ImplDX10_Init(pDevice);

		init = true;
	}

	ImGui_ImplDX10_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	impl::MenuLoop();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

	return oPresent(pSwapChain, SyncInterval, Flags);
}

void impl::d3d10::init()
{
	kiero::bind(8, (void**)&oPresent, hkPresent10);
}

#endif // KIERO_INCLUDE_D3D10