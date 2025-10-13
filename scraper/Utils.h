#pragma once
#include <Windows.h>
#include <string>

uintptr_t GetModuleBase(const wchar_t* moduleName);

std::string GetTeamName(int teamId);
std::string ResolveTeamName(void* playerStruct);
