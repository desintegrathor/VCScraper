#pragma once
#include <Windows.h>

bool InitKillfeedHook(uintptr_t baseGameDll);
void RemoveKillfeedHook();
