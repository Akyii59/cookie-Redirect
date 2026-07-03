#include "pch.h"
#include "Settings.h"
#include "Patcher.h"

void __cdecl Startup()
{
    Cookie::Core::Initialize();
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (Cookie::Settings.ManualMapping)
            Startup();
        else
        {
            HANDLE thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Startup, nullptr, 0, nullptr);
            if (thread) CloseHandle(thread);
        }
    }
    return TRUE;
}
