#include "../../../kiero.h"

#include "win32_impl.h"
#include <Windows.h>
#include <iostream>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"

#include "ImGuiInjector/ImGuiInjector.hpp"

void impl::win32::init(HWND hWnd)
{
	ImGuiInjector::Get().SetWndProcHook(hWnd);
}