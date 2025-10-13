#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

static HANDLE hPipe = INVALID_HANDLE_VALUE;
static std::mutex pipeMutex;
static bool pipeReady = false;

constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\GameDataPipe";

// ?? Pomocn� funkce: pokus o p�ipojen� (rekurzivn� �ek�, pokud je pipe busy)
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
            // Pipe existuje, ale je obsazen� � �ek�me max 2 sekundy
            if (WaitNamedPipeW(PIPE_NAME, 2000))
                return TryConnectPipe(); // zkus znovu po �ek�n�
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

// ?? Inicializace spojen�
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

// ?? Odesl�n� zpr�vy (automaticky se pokus� o reconnect, pokud dojde ke ztr�t�)
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

        // Pokud se spojen� p�eru�ilo, uzav�eme a zkus�me se znovu p�ipojit
        std::cerr << "[PipeSender] Chyba pri zapisu do pipe (" << err << "), pokus o reconnect..." << std::endl;
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
        pipeReady = false;

        // zkus reconnectovat (asynchronn�, a� nezdr�uje hlavn� thread)
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            TryConnectPipe();
            }).detach();
    }
    else
    {
        // Debug logy podle typu zpr�vy (voliteln�)
        if (msg.find("\"type\":\"flag\"") != std::string::npos)
            std::cout << "[PipeSender] -> FLAG update sent." << std::endl;
        else if (msg.find("\"type\":\"scoreboard\"") != std::string::npos)
            std::cout << "[PipeSender] -> SCOREBOARD update sent." << std::endl;
        else if (msg.find("\"type\":\"kill\"") != std::string::npos)
            std::cout << "[PipeSender] -> KILL event sent." << std::endl;
    }
}
