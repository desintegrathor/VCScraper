#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <iomanip>
#include <vector>
#include <sstream>
#include "nlohmann/json.hpp"


// nlohmann/json - JSON parser
#include "nlohmann/json.hpp"
using json = nlohmann::json;

constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\GameDataPipe";

struct Player {
    int id;
    std::string name;
    std::string team;
    int kills;
    int deaths;
    int score;
    int ping;
};

void PrintScoreboard(const json& j)
{
    if (!j.contains("players")) return;

    int totalKills = j.value("totalKills", 0);
    int playerCount = j.value("playerCount", 0);

    std::cout << "\n\033[36m=== SCOREBOARD ===\033[0m\n";
    std::cout << "Players: " << playerCount << " | Total Kills: " << totalKills << "\n";

    std::cout << std::left
        << std::setw(4) << "ID"
        << std::setw(20) << "Name"
        << std::setw(8) << "Team"
        << std::setw(6) << "K"
        << std::setw(6) << "D"
        << std::setw(6) << "S"
        << std::setw(6) << "Ping"
        << "\n---------------------------------------------\n";

    for (auto& p : j["players"])
    {
        std::cout << std::left
            << std::setw(4) << p.value("id", 0)
            << std::setw(20) << p.value("name", "")
            << std::setw(8) << p.value("team", "")
            << std::setw(6) << p.value("kills", 0)
            << std::setw(6) << p.value("deaths", 0)
            << std::setw(6) << p.value("score", 0)
            << std::setw(6) << p.value("ping", 0)
            << "\n";
    }
}

void PrintFlagStatus(const json& j)
{
    int us = j.value("US", -1);
    int vc = j.value("VC", -1);

    std::cout << "\033[33m[FLAG]\033[0m US=" << us << " | VC=" << vc << std::endl;
}

void PrintKillEvent(const json& j)
{
    int killer = j.value("killer", -1);
    int victim = j.value("victim", -1);

    std::cout << "\033[31m[KILL]\033[0m Killer=" << killer
        << " -> Victim=" << victim << std::endl;
}

void PipeReceiverThread()
{
    HANDLE hPipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 0, 0, 0, NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "[Receiver] CreateNamedPipe selhalo: " << GetLastError() << std::endl;
        return;
    }

    std::cout << "[Receiver] Cekam na spojeni s hlavnim DLL..." << std::endl;

    if (!ConnectNamedPipe(hPipe, NULL)) {
        std::cerr << "[Receiver] ConnectNamedPipe selhalo: " << GetLastError() << std::endl;
        CloseHandle(hPipe);
        return;
    }

    std::cout << "\033[32m[Receiver] Klient pripojen!\033[0m" << std::endl;

    char buffer[8192];
    DWORD bytesRead;

    std::string dataChunk;

    while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
    {
        if (bytesRead == 0) continue;
        buffer[bytesRead] = 0;

        dataChunk.append(buffer, bytesRead);

        // JSONy pøichází oddìlenì - zkusíme je zpracovat po øádcích nebo ukonèovacích znacích
        size_t pos;
        while ((pos = dataChunk.find('}')) != std::string::npos)
        {
            std::string jsonStr = dataChunk.substr(0, pos + 1);
            dataChunk.erase(0, pos + 1);

            try {
                json j = json::parse(jsonStr);

                std::string type = j.value("type", "");

                if (type == "flag") {
                    PrintFlagStatus(j);
                }
                else if (type == "scoreboard") {
                    PrintScoreboard(j);
                }
                else if (type == "kill") {
                    PrintKillEvent(j);
                }
                else {
                    std::cout << "[UNKNOWN] " << jsonStr << std::endl;
                }
            }
            catch (const std::exception& e) {
                // ignorujeme neúplné JSONy (èekají na další data)
            }
        }
    }

    std::cout << "[Receiver] Pipe ukonceno." << std::endl;
    CloseHandle(hPipe);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"GameData Receiver v2");

    std::cout << "=============================================\n";
    std::cout << "   GAME DATA RECEIVER v2\n";
    std::cout << "   Waiting for DLL connection...\n";
    std::cout << "=============================================\n";

    PipeReceiverThread();
    return 0;
}
