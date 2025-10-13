#include "Utils.h"

uintptr_t GetModuleBase(const wchar_t* moduleName)
{
    HMODULE hMod = GetModuleHandleW(moduleName);
    if (!hMod) return 0;
    return reinterpret_cast<uintptr_t>(hMod);
}
std::string GetTeamName(int teamId)
{
    switch (teamId) {
    case 1: return "VC";
    case 0: return "US";
    default: return "Unknown";
    }
}

std::string ResolveTeamName(void* playerStruct)
{
    if (!playerStruct) return "Unknown";

    int teamId = *((int*)playerStruct + 5);
    std::string teamName = GetTeamName(teamId);

    const char* name = (const char*)playerStruct + 40;
    std::string playerName(name);

    if (teamId == 0 && playerName.rfind("Spectator", 0) == 0)
        teamName = "Spectator";

    return teamName;
}