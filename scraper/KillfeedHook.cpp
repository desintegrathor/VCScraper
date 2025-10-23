#include "KillfeedHook.h"
#include "PipeSender.h"
#include "minhook-master/include/minhook.h"
#include <iostream>
#include <sstream>

// Externí callback z dllmain.cpp
extern void OnKillEvent(int killerId, int victimId);

typedef int(__cdecl* tKillMsg)(int a1, int a2);
static tKillMsg oKillMsg = nullptr;

static int __cdecl hkKillMsg(int a1, int a2)
{
    int victimId = *(int*)(a1 + 2);
    int killerId = *(int*)(a1 + 6);

    OnKillEvent(killerId, victimId);

    return oKillMsg(a1, a2);
}

bool InitKillfeedHook(uintptr_t baseGameDll)
{
    if (MH_Initialize() != MH_OK)
        return false;

    void* target = (void*)(baseGameDll + 0x161E50);

    if (MH_CreateHook(target, &hkKillMsg, reinterpret_cast<LPVOID*>(&oKillMsg)) != MH_OK)
        return false;

    if (MH_EnableHook(target) != MH_OK)
        return false;

    std::cout << "[Killfeed] Hook aktivni (posila kill event)" << std::endl;
    return true;
}

void RemoveKillfeedHook()
{
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
