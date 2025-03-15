#include "../../../kiero.h"

#if KIERO_INCLUDE_D3D9

#include "d3d9_impl.h"
#include <d3d9.h>
#include <assert.h>
#include <iostream>

#include "win32_impl.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx9.h"

#include "Helpers/Helpers.hpp"

typedef long(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
static Reset oReset = NULL;

typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
static EndScene oEndScene = NULL;

long __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (pDevice == NULL) {
		return oReset(pDevice, pPresentationParameters);
	}

	D3DDEVICE_CREATION_PARAMETERS pParameters{ 0 };
	ImGui_ImplDX9_InvalidateDeviceObjects();
	long result = oReset(pDevice, pPresentationParameters);
	ImGui_ImplDX9_CreateDeviceObjects();

	return result;
}

long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	static bool init = false;

	if (!init)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		D3DDEVICE_CREATION_PARAMETERS params;
		pDevice->GetCreationParameters(&params);

		HWND TargetHwnd = params.hFocusWindow;
		assert(TargetHwnd != NULL && "No window handle in hkEndScene()");
		impl::win32::init(TargetHwnd);

		ImGui_ImplWin32_Init(TargetHwnd);
		ImGui_ImplDX9_Init(pDevice);

		ImGuiViewport* vp = ImGui::GetMainViewport();
		assert(vp != NULL && "ImGuiViewport was nullptr in khEndScene()");
		vp->PlatformHandleRaw = reinterpret_cast<void*>(TargetHwnd);

		init = true;
	}

	IDirect3DSurface9* backbuffer;

	if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)))
	{
		pDevice->SetRenderTarget(0, backbuffer);
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		impl::MenuLoop();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

		backbuffer->Release();
	}

	return oEndScene(pDevice);
}

void impl::d3d9::init()
{
	kiero::bind(16, (void**)&oReset, hkReset);
	kiero::bind(42, (void**)&oEndScene, hkEndScene);
}

#endif // KIERO_INCLUDE_D3D9