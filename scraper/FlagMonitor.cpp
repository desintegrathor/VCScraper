#include <Windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include "PipeSender.h"
#include "Utils.h"


uintptr_t GetModuleBase(const wchar_t* moduleName);

void MonitorFlags()
{
    std::cout << "[FlagMonitor] Inicializace sledovani vlajek..." << std::endl;

    uintptr_t baseGame = 0;
    for (int i = 0; i < 50 && !baseGame; i++)
    {
        baseGame = GetModuleBase(L"game.dll");
        if (!baseGame)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!baseGame)
    {
        std::cout << "[FlagMonitor] Nepodarilo se najit game.dll ani po cekani." << std::endl;
        return;
    }

    std::cout << "[FlagMonitor] Base adresa game.dll = 0x"
        << std::hex << baseGame << std::dec << std::endl;

    const uintptr_t US_FLAG_OFFSET = 0x80DDE0;
    const uintptr_t VC_FLAG_OFFSET = 0x80DDE4;

    int lastUS = -1, lastVC = -1;

    std::cout << "[FlagMonitor] Sleduji vlajky (US/VC)..." << std::endl;

    while (true)
    {
        // Bezpeènì ovìøíme, že lze èíst
        if (IsBadReadPtr((void*)(baseGame + US_FLAG_OFFSET), sizeof(int)) ||
            IsBadReadPtr((void*)(baseGame + VC_FLAG_OFFSET), sizeof(int)))
        {
            std::cout << "\n[FlagMonitor] Pamet nedostupna, mozna hra konci." << std::endl;
            break;
        }

        int usFlagHolder = *(int*)(baseGame + US_FLAG_OFFSET);
        int vcFlagHolder = *(int*)(baseGame + VC_FLAG_OFFSET);

        if (usFlagHolder != lastUS || vcFlagHolder != lastVC)
        {
            std::ostringstream json;
            json << "{ \"type\":\"flag\", \"US\":" << usFlagHolder
                << ", \"VC\":" << vcFlagHolder << " }";
            SendPipeMessage(json.str());

            lastUS = usFlagHolder;
            lastVC = vcFlagHolder;

            std::cout << "\r[FlagMonitor] US_flag=" << usFlagHolder
                << " | VC_flag=" << vcFlagHolder
                << "        " << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[FlagMonitor] Vlákno ukonceno." << std::endl;
}
