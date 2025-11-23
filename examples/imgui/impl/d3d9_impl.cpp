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
#include "ImGuiInjector/ImGuiInjector.hpp"

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
	static HWND TargetHwnd = 0;
	static LPDIRECT3DDEVICE9 device = 0;
	static ImGuiContext* ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(ctx);

	if (!init)
	{
		IMGUI_CHECKVERSION();

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
			ImGui_ImplDX9_Init(pDevice);
		}
	}

	IDirect3DSurface9* backbuffer;

	ImGuiInjector::Get().UpdateGlobalScale();
	ImGuiInjector::Get().ResetInput();
	if (!ImGuiInjector::Get().IsMenuRunning()) return oEndScene(pDevice);

	IDirect3DSurface9* oldTarget;
	pDevice->GetRenderTarget(0, &oldTarget);
	if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)))
	{
		pDevice->SetRenderTarget(0, backbuffer);
		D3DSURFACE_DESC desc;
		backbuffer->GetDesc(&desc);
		ImVec2 canvasSize(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
		ImGuiInjector::Get().SetCanvasSize(canvasSize);
		pDevice->SetRenderTarget(0, backbuffer);
		ImGui_ImplDX9_NewFrame();
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
		ImGui_ImplDX9_RenderDrawData(draw_data);

		backbuffer->Release();

		pDevice->SetRenderTarget(0, oldTarget);
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