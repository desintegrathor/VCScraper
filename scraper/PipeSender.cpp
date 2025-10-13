#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

static HANDLE hPipe = INVALID_HANDLE_VALUE;
static std::mutex pipeMutex;
static bool pipeReady = false;

constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\GameDataPipe";

// ?? Pomocná funkce: pokus o pøipojení (rekurzivnì èeká, pokud je pipe busy)
bool TryConnectPipe()
{
    hPipe = CreateFileW(
        PIPE_NAME,
        GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();

        if (err == ERROR_PIPE_BUSY)
        {
            // Pipe existuje, ale je obsazená — èekáme max 2 sekundy
            if (WaitNamedPipeW(PIPE_NAME, 2000))
                return TryConnectPipe(); // zkus znovu po èekání
        }
        else if (err != ERROR_FILE_NOT_FOUND)
        {
            std::cerr << "[PipeSender] CreateFileW selhalo (" << err << ")" << std::endl;
        }

        return false;
    }

    pipeReady = true;
    std::cout << "[PipeSender] Spojeno s receiverem (" << PIPE_NAME << ")" << std::endl;
    return true;
}

// ?? Inicializace spojení
bool InitPipeSender()
{
    std::cout << "[PipeSender] Pokus o pripojeni k receiveru..." << std::endl;

    if (!TryConnectPipe())
    {
        std::cout << "[PipeSender] Pipe neni zatim dostupna, zkusim znovu pozdeji." << std::endl;
        return false;
    }

    return true;
}

// ?? Odeslání zprávy (automaticky se pokusí o reconnect, pokud dojde ke ztrátì)
void SendPipeMessage(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(pipeMutex);

    if (!pipeReady)
    {
        if (!TryConnectPipe())
        {
            std::cerr << "[PipeSender] Nelze odeslat, pipe neni dostupna." << std::endl;
            return;
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) return;

    DWORD bytesWritten = 0;
    BOOL success = WriteFile(hPipe, msg.c_str(), (DWORD)msg.size(), &bytesWritten, NULL);

    if (!success || bytesWritten != msg.size())
    {
        DWORD err = GetLastError();

        // Pokud se spojení pøerušilo, uzavøeme a zkusíme se znovu pøipojit
        std::cerr << "[PipeSender] Chyba pri zapisu do pipe (" << err << "), pokus o reconnect..." << std::endl;
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
        pipeReady = false;

        // zkus reconnectovat (asynchronnì, a nezdržuje hlavní thread)
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            TryConnectPipe();
            }).detach();
    }
    else
    {
        // Debug logy podle typu zprávy (volitelné)
        if (msg.find("\"type\":\"flag\"") != std::string::npos)
            std::cout << "[PipeSender] -> FLAG update sent." << std::endl;
        else if (msg.find("\"type\":\"scoreboard\"") != std::string::npos)
            std::cout << "[PipeSender] -> SCOREBOARD update sent." << std::endl;
        else if (msg.find("\"type\":\"kill\"") != std::string::npos)
            std::cout << "[PipeSender] -> KILL event sent." << std::endl;
    }
}
