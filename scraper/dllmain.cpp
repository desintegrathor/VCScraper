#include <Windows.h>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <unordered_map>
#include "KillfeedHook.h"
#include "PipeSender.h"
#include "FlagMonitor.h"
#include "Utils.h"

std::string ResolveTeamName(void* playerStruct);


// Lokální cache hráčů pro sledování změn
struct PlayerStats {
    int kills;
    int deaths;
    int score;
};

static std::unordered_map<int, PlayerStats> lastPlayerData;

// Předeklarace
void SendFullScoreboard(uintptr_t baseGame);
void OnKillEvent(int killerId, int victimId);

// ---------------------------
//  Hook Callback pro killfeed
// ---------------------------
void OnKillEvent(int killerId, int victimId)
{
    // Poslat kill event
    std::ostringstream killJson;
    killJson << "{ \"type\":\"kill\", \"killer\":" << killerId << ", \"victim\":" << victimId << " }";
    SendPipeMessage(killJson.str());

    // Po každém killu pošli i kompletní scoreboard
    uintptr_t baseGame = GetModuleBase(L"game.dll");
    SendFullScoreboard(baseGame);
}

// ---------------------------
//  Scoreboard builder
// ---------------------------
void SendFullScoreboard(uintptr_t baseGame)
{
    void** playerTable = (void**)(baseGame + 0x7AE9C8);

    std::ostringstream json;
    json << "{ \"type\":\"scoreboard\", \"players\":[";

    int totalKills = 0;
    int playerCount = 0;

    for (int i = 0; i < 64; i++)
    {
        void* entry = playerTable[i];
        if (!entry) continue;

        playerCount++;

        int playerId = *(int*)entry;
        const char* name = (const char*)entry + 40;
        int kills = *((int*)entry + 2912);
        int deaths = *((int*)entry + 2913);
        int score = *((int*)entry + 2914);
        int ping = *((int*)entry + 3040);

        std::string teamName = ResolveTeamName(entry);
        totalKills += kills;

        json << "{"
            << "\"id\":" << playerId << ","
            << "\"name\":\"" << name << "\","
            << "\"team\":\"" << teamName << "\","
            << "\"kills\":" << kills << ","
            << "\"deaths\":" << deaths << ","
            << "\"score\":" << score << ","
            << "\"ping\":" << ping
            << "},";

        // Ulož do cache
        lastPlayerData[playerId] = { kills, deaths, score };
    }

    if (playerCount > 0)
        json.seekp(-1, std::ios_base::end); // odstraníme poslední čárku

    json << "], \"totalKills\":" << totalKills
        << ", \"playerCount\":" << playerCount
        << " }";

    SendPipeMessage(json.str());
}

// ---------------------------
//  Sledování změn ve scoreboardu
// ---------------------------
void MonitorScoreChanges(uintptr_t baseGame)
{
    void** playerTable = (void**)(baseGame + 0x7AE9C8);

    while (true)
    {
        int currentPlayerCount = 0;

        for (int i = 0; i < 64; i++)
        {
            void* entry = playerTable[i];
            if (!entry) continue;

            currentPlayerCount++;

            int playerId = *(int*)entry;
            int kills = *((int*)entry + 2912);
            int deaths = *((int*)entry + 2913);
            int score = *((int*)entry + 2914);

            auto it = lastPlayerData.find(playerId);

            if (it == lastPlayerData.end())
            {
                // Nový hráč -> přidat do cache
                lastPlayerData[playerId] = { kills, deaths, score };
                continue;
            }

            // Sleduj pouze změnu SCORE (kills a deaths ignorujeme)
            if (it->second.score != score)
            {
                std::ostringstream update;
                update << "{ \"type\":\"playerUpdate\", \"changedId\":" << playerId << " }";
                SendPipeMessage(update.str());

                SendFullScoreboard(baseGame);
                it->second = { kills, deaths, score };
            }
            else
            {
                // Udržuj aktuální hodnoty kills/deaths kvůli cache synchronizaci
                it->second.kills = kills;
                it->second.deaths = deaths;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}


// ---------------------------
//  DLL Entry
// ---------------------------
DWORD WINAPI MainThread(LPVOID)
{
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "[DLL] Konzole zapnuta" << std::endl;

    uintptr_t baseGame = GetModuleBase(L"game.dll");
    if (!baseGame) {
        std::cerr << "[DLL] game.dll nenalezen" << std::endl;
        return 0;
    }

    // Připojení k receiveru
    if (!InitPipeSender()) {
        std::cerr << "[DLL] Nepodarilo se pripojit k receiveru" << std::endl;
    }

    // Aktivace killfeed hooku
    InitKillfeedHook(baseGame);

    // Start monitoringu vlajek
    std::thread flagThread(MonitorFlags);
    flagThread.detach();

    // Start sledování změn ve scoreboardu
    std::thread scoreboardThread([=]() { MonitorScoreChanges(baseGame); });
    scoreboardThread.detach();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        RemoveKillfeedHook();
    }
    return TRUE;
}
