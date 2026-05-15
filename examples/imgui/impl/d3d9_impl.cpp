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

typedef long(__stdcall* ResetEx)(LPDIRECT3DDEVICE9EX, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
static ResetEx oResetEx = NULL;

typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
static EndScene oEndScene = NULL;

typedef long(__stdcall* Present)(LPDIRECT3DSWAPCHAIN9, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
static Present oPresent = NULL;

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

long __stdcall hkResetEx(LPDIRECT3DDEVICE9EX pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX* pFullscreenDisplayMode)
{
	if (pDevice == NULL) {
		return oResetEx(pDevice, pPresentationParameters, pFullscreenDisplayMode);
	}
	ImGui_ImplDX9_InvalidateDeviceObjects();
	long result = oResetEx(pDevice, pPresentationParameters, pFullscreenDisplayMode);
	ImGui_ImplDX9_CreateDeviceObjects();

	return result;
}

struct SceneDesc {
	struct int2 {
		int x = 0;
		int y = 0;

		auto operator<=>(const int2&) const = default;
	};
	int2 canvasSize;
	int2 outputSize;
	SceneDesc(int canvasX, int canvasY, int outputX, int outputY) :
		canvasSize(canvasX, canvasY), outputSize(outputX, outputY) {
	};
};

static int sceneIdx = -1;
static int sceneTarget = -1;
static std::vector<SceneDesc> scenes;
long __stdcall hkPresent(LPDIRECT3DSWAPCHAIN9 pSwapchain, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags) {
	if (!scenes.empty()) {
		int target = scenes.size() - 1;
		if (scenes.size() > 1) {
			if (scenes[target].canvasSize != scenes[target - 1].canvasSize) target--;
		}
		sceneTarget = target;
		scenes.clear();
	}
	sceneIdx = -1;
	return oPresent(pSwapchain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

long __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	static bool init = false;
	static HWND TargetHwnd = 0;
	static LPDIRECT3DDEVICE9 device = 0;
	static int rtMax = 1;
	static ImGuiContext* ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(ctx);

	if (!init)
	{
		IMGUI_CHECKVERSION();

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

	D3DSURFACE_DESC canvas{};
	{
		IDirect3DSurface9* rt{};
		pDevice->GetRenderTarget(0, &rt);
		rt->GetDesc(&canvas);
		rt->Release();
	}
	D3DSURFACE_DESC output{};
	{
		IDirect3DSurface9* backbuffer{};
		pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
		backbuffer->GetDesc(&output);
		backbuffer->Release();
	}
	scenes.emplace_back(canvas.Width, canvas.Height, output.Width, output.Height);

	sceneIdx++;
	if (sceneIdx != sceneTarget) return oEndScene(pDevice);

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

	ImGuiInjector::Get().UpdateInput();

	return oEndScene(pDevice);
}

void impl::d3d9::init()
{
	kiero::bind(16, (void**)&oReset, hkReset);
	kiero::bind(42, (void**)&oEndScene, hkEndScene);
	kiero::bind(132, (void**)&oResetEx, hkResetEx);
	kiero::bind(133 + 3, (void**)&oPresent, hkPresent);
}

#endif // KIERO_INCLUDE_D3D9