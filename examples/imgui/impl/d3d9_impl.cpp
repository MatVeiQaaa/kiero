#include "../../../kiero.h"

#if KIERO_INCLUDE_D3D9

#include "d3d9_impl.h"
#include <d3d9.h>
#include <assert.h>
#include <iostream>
#include <array>

#include "win32_impl.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx9.h>
#include <safetyhook.hpp>

#include <Helpers/Helpers.hpp>
#include <ImGuiInjector/ImGuiInjector.hpp>

typedef long(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
static Reset oReset = NULL;

typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
static EndScene oEndScene = NULL;

long __stdcall hkReset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (pDevice == NULL) {
		return oReset(pDevice, pPresentationParameters);
	}
	ImGui_ImplDX9_InvalidateDeviceObjects();
	long result = oReset(pDevice, pPresentationParameters);
	ImGui_ImplDX9_CreateDeviceObjects();

	return result;
}

static int sceneIdx = 0;
static safetyhook::InlineHook oPresent;
long __stdcall hkPresent(LPDIRECT3DSWAPCHAIN9 pSwapchain, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags) {
	sceneIdx = 0;
	return oPresent.stdcall<long>(pSwapchain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	int curSceneIdx = sceneIdx;
	sceneIdx++;
	if (curSceneIdx != 0) return oEndScene(pDevice);

	static bool init = false;
	static HWND TargetHwnd = 0;
	static LPDIRECT3DDEVICE9 device = 0;
	static int rtMax = 1;
	static ImGuiContext* ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(ctx);

	if (!init)
	{
		IMGUI_CHECKVERSION();

		IDirect3DSwapChain9* swapchain{};
		pDevice->GetSwapChain(0, &swapchain);
		oPresent = safetyhook::create_inline((*(uintptr_t**)swapchain)[3], hkPresent);

		D3DCAPS9 caps{};
		pDevice->GetDeviceCaps(&caps);
		rtMax = caps.NumSimultaneousRTs;

		D3DDEVICE_CREATION_PARAMETERS params;
		pDevice->GetCreationParameters(&params);

		TargetHwnd = params.hFocusWindow;
		device = pDevice;
		assert(TargetHwnd != NULL && "No window handle in hkEndScene()");
		impl::win32::init(TargetHwnd);

		ImGui_ImplWin32_Init(TargetHwnd);
		ImGui_ImplDX9_Init(pDevice);

		ImGuiViewport* vp = ImGui::GetMainViewport();
		assert(vp != NULL && "ImGuiViewport was nullptr in khEndScene()");
		vp->PlatformHandleRaw = reinterpret_cast<void*>(TargetHwnd);

		ImGuiInjector::Get().LoadJapaneseFont();
		ImGuiInjector::Get().SetStartingStyle(ImGui::GetStyle());

		init = true;
	}

	{
		D3DDEVICE_CREATION_PARAMETERS params;
		pDevice->GetCreationParameters(&params);
		if (TargetHwnd != params.hFocusWindow) {
			TargetHwnd = params.hFocusWindow;
			ImGui_ImplWin32_Shutdown();
			ImGui_ImplWin32_Init(TargetHwnd);
		}
		if (device != pDevice) {
			device = pDevice;
			ImGui_ImplDX9_Shutdown();
			ImGui_ImplDX9_Init(device);
		}
	}

	ImGuiInjector::Get().UpdateGlobalScale();
	ImGuiInjector::Get().ResetInput();
	static bool wasReset = false;
	if (!ImGuiInjector::Get().IsMenuRunning() || IsIconic(TargetHwnd)) {
		if (!wasReset) {
			wasReset = true;
			ImGui::GetIO().ClearInputKeys();
			ImGui::GetIO().ClearInputMouse();
		}
		return oEndScene(pDevice);
	}
	wasReset = false;

	std::vector<IDirect3DSurface9*> rts{};
	rts.reserve(rtMax);
	for (int i = 0; i < rts.capacity(); i++) {
		IDirect3DSurface9* rt{};
		device->GetRenderTarget(i, &rt);
		rts.push_back(rt);
		if (i != 0) pDevice->SetRenderTarget(i, NULL);
	}
	D3DSURFACE_DESC canvas{};
	rts[0]->GetDesc(&canvas);

	IDirect3DSurface9* backbuffer{};
	D3DSURFACE_DESC output{};
	device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
	backbuffer->GetDesc(&output);
	backbuffer->Release();

	ImVec2 canvasF({ static_cast<float>(canvas.Width), static_cast<float>(canvas.Height) });

	ImGuiInjector::Get().SetCanvasSize(canvasF);
	ImGuiInjector::Get().SetOutputSize({ static_cast<float>(output.Width), static_cast<float>(output.Height) });

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::GetIO().DisplaySize = canvasF;

	ImGui::NewFrame();

	impl::MenuLoop();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	for (int i = 0; i < rts.size(); i++) {
		auto& tar = rts[i];
		pDevice->SetRenderTarget(i, tar);
		if (tar) tar->Release();
	}
	ImGuiInjector::Get().UpdateInput();

	return oEndScene(pDevice);
}

void impl::d3d9::init()
{
	kiero::bind(16, (void**)&oReset, hkReset);
	kiero::bind(42, (void**)&oEndScene, hkEndScene);
}

#endif // KIERO_INCLUDE_D3D9