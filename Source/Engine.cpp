#include "pch.h"
#include "Engine.h"
#include "URI.h"
#include "Settings.h"

namespace Cookie
{
    int g_MemLeakCount = 0;
    bool (*ProcessRequestOG)(FCurlHttpRequest*) = nullptr;
    bool (*ProcessRequestOG_EOS)(FCurlHttpRequest*) = nullptr;
    void FCurlHttpRequest::InitializeURLIndex()
    {
        auto firstFunc = (uint64_t)(*VTable);
        uint32_t urlFieldOffset = 0;

        for (int i = 0; i < 100; i++)
            if (Cookie::Patch::CheckBytes(firstFunc, i, { 0x48, 0x8D, 0x91 }))
            {
                urlFieldOffset = *(uint32_t*)(firstFunc + i + 3);
                break;
            }

        if (!urlFieldOffset)
        {
            SetURLIdx = 10;
            return;
        }

        for (int64_t i = 1; i < 0x20; i++)
        {
            auto func = (uint64_t)VTable[i];
            for (int j = 0; j < 0x20; j++)
                if (Cookie::Patch::CheckBytes(func, j, { 0x48, 0x81, 0xC1 }))
                    if (*(uint32_t*)(func + j + 3) == urlFieldOffset)
                    {
                        SetURLIdx = i;
                        return;
                    }
        }
        SetURLIdx = 10;
    }

    FString FCurlHttpRequest::GetURL()
    {
        return ((FString& (*)(FCurlHttpRequest*, FString))VTable[0])(this, FString());
    }

    void FCurlHttpRequest::RedirectRequest(bool bEOS)
    {
        if (!SetURLIdx && !bEOS)
            InitializeURLIndex();
        else if (++g_MemLeakCount == 5 && Settings.PatchMemoryLeak)
        {
            const uint8_t p1[] = { 0x48,0x89,0x5C,0x24,0xCC,0x57,0x48,0x83,0xEC,0xCC,0x48,0x8B,0x01,0x4C,0x8B,0xC2,0x48,0x8D,0x54,0x24,0xCC,0x48,0x8B,0xD9,0xFF,0x50,0x30 };
            const char   m1[] = "xxxx?xxxx?xxxxxxxxxxxx?xxxxxxx";
            const uint8_t p2[] = { 0x48,0x8B,0x01,0x4C,0x8D,0x41,0x08,0x48,0xFF,0x60,0x20 };
            const char   m2[] = "xxxxxxxxxxx";

            auto addr = Cookie::Patch::FindPattern(p1, m1, sizeof(p1));
            if (!addr) addr = Cookie::Patch::FindPattern(p2, m2, sizeof(p2));

            if (addr)
            {
                DWORD old;
                VirtualProtect((LPVOID)addr, 1, PAGE_EXECUTE_READWRITE, &old);
                *(uint8_t*)addr = 0xC3;
                VirtualProtect((LPVOID)addr, 1, old, &old);
            }
        }

        auto url = GetURL();
        URL parsed(url);

        if (Filter::Check(parsed))
        {
            if (Settings.UseCommandLine)
                parsed.SetHost(Settings.Backend);
            else
                parsed.SetHost(Settings.Backend);

            FString rebuilt = parsed.Build();
            ((void (*)(FCurlHttpRequest*, FString))VTable[bEOS ? 10 : SetURLIdx])(this, rebuilt);
            rebuilt.Dealloc();

            parsed.DeallocPathQuery();
        }
        else
        {
            parsed.Dealloc();
        }
        url.Dealloc();
    }
}
