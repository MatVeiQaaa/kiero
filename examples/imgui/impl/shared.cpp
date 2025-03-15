#define _CRT_SECURE_NO_WARNINGS

#include "shared.h"
#include <stdio.h>
#include "imgui/imgui.h"

#include "ImGuiInjector/ImGuiInjector.hpp"

void impl::MenuLoop()
{
	ImGuiInjector::Get().RunMenus();
}