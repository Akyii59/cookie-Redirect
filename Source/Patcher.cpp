#include "pch.h"
#include "Patcher.h"
#include "Settings.h"
#include "Scan.h"
#include "Engine.h"
#include "URI.h"
#include "Console.h"

namespace Cookie
{
    static bool HookVTable(void* hookFunc, void** original, bool eosModule)
    {
        auto base = eosModule ? Cookie::PE::GetBase(L"EOSSDK-Win64-Shipping") : Cookie::PE::ImageBase;

        auto savedBase = Cookie::PE::ImageBase;
        if (eosModule) Cookie::PE::ImageBase = base;

        auto ref = Cookie::Patch::FindStringRef(L"STAT_FCurlHttpRequest_ProcessRequest");
        if (!ref) ref = Cookie::Patch::FindStringRef(L"%p: request (easy handle:%p) has been added to threaded queue for processing");
        if (!ref) ref = Cookie::Patch::FindStringRef("STAT_FCurlHttpRequest_ProcessRequest");
        if (!ref)
        {
            if (eosModule) Cookie::PE::ImageBase = savedBase;
            return false;
        }

        uint64_t funcAddr = 0;
        for (int i = 0; i < 2048; i++)
        {
            if (eosModule)
            {
                if (Cookie::Patch::CheckBytes(ref, i, { 0x48, 0x89, 0x5C }, true))
                {
                    funcAddr = ref - i;
                    break;
                }
            }
            else
            {
                if (Cookie::Patch::CheckBytes(ref, i, { 0x4C, 0x8B, 0xDC }, true) ||
                    Cookie::Patch::CheckBytes(ref, i, { 0x48, 0x8B, 0xC4 }, true))
                {
                    funcAddr = ref - i;
                    break;
                }

                if (Cookie::Patch::CheckBytes(ref, i, { 0x48, 0x81, 0xEC }, true) ||
                    Cookie::Patch::CheckBytes(ref, i, { 0x48, 0x83, 0xEC }, true))
                {
                    for (int x = 0; x < 50; x++)
                    {
                        if (Cookie::Patch::CheckBytes(ref, i + x, { 0x40 }, true))
                        {
                            funcAddr = ref - i - x;
                            goto located;
                        }
                        if (Cookie::Patch::CheckBytes(ref, i + x, { 0x4C, 0x8B, 0xDC }, true) ||
                            Cookie::Patch::CheckBytes(ref, i + x, { 0x48, 0x8B, 0xC4 }, true) ||
                            Cookie::Patch::CheckBytes(ref, i + x, { 0x48, 0x89, 0x5C }, true))
                            break;
                    }
                }
            }
        }

        if (!funcAddr)
        {
            if (eosModule) Cookie::PE::ImageBase = savedBase;
            return false;
        }

    located:
        auto rdataSect = Cookie::PE::GetSection(".rdata");
        if (!rdataSect)
        {
            if (eosModule) Cookie::PE::ImageBase = savedBase;
            return false;
        }

        auto rdataStart = (uint8_t*)(Cookie::PE::ImageBase + rdataSect->VirtualAddress);
        auto rdataSize = rdataSect->Misc.VirtualSize;

        uint64_t vftSlot = 0;
        __m128i wide = _mm_set1_epi32((int)(funcAddr & 0xFFFFFFFF));

        for (uint32_t i = 0; i < rdataSize - (rdataSize % 16); i += 16)
        {
            auto data = _mm_load_si128((const __m128i*)(rdataStart + i));
            int mask = _mm_movemask_epi8(_mm_cmpeq_epi32(data, wide));
            if (!mask) continue;

            for (int q = 0; q < 16; q += 4)
            {
                if (mask & (1 << q))
                {
                    auto entry = (uint64_t*)(rdataStart + i + q);
                    if (*entry == funcAddr)
                    {
                        vftSlot = (uint64_t)entry;
                        goto hooked;
                    }
                }
            }
        }

        if (eosModule) Cookie::PE::ImageBase = savedBase;
        return false;

    hooked:
        DWORD oldProtect;
        VirtualProtect((LPVOID)vftSlot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        if (original) *original = (void*)funcAddr;
        *(void**)vftSlot = hookFunc;
        VirtualProtect((LPVOID)vftSlot, sizeof(void*), oldProtect, &oldProtect);

        if (eosModule) Cookie::PE::ImageBase = savedBase;
        return true;
    }

    static bool InterceptRequest(FCurlHttpRequest* req)
    {
        req->RedirectRequest(false);
        return ProcessRequestOG(req);
    }

    static bool InterceptEOS(FCurlHttpRequest* req)
    {
        req->RedirectRequest(true);
        return ProcessRequestOG_EOS(req);
    }

    namespace Core
    {
        void Initialize()
        {
            if (Settings.ShowWindow)
            {
                AllocConsole();
                FILE* fp = nullptr;
                freopen_s(&fp, "CONOUT$", "w+", stdout);
                SetConsoleTitleA("Cookie Redirect | made by Akyi");
            }

            const uint8_t reallocSig[] = { 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0xCC,0x48,0x8B,0xF1,0x41,0x8B,0xD8,0x48,0x8B,0x0D };
            const char   reallocMask[] = "xxxxxxxxxxxxxx?xxxxxxxx";

            while (!Cookie::FMemory__Realloc)
                Cookie::FMemory__Realloc = Cookie::Patch::FindPattern(reallocSig, reallocMask, sizeof(reallocSig));

            Cookie::Settings.Backend = Cookie::FString(Cookie::Settings.BackendURL);

            if (Settings.UseCommandLine)
            {
                Cookie::FString cmd = GetCommandLineW();
                auto pos = cmd.Find(L"-backend=");
                if (pos != Cookie::FString::NotFound)
                {
                    auto val = cmd.Substr(pos + 9);
                    Cookie::Settings.Backend = val;
                    val.Dealloc();
                }
                cmd.Dealloc();
            }

            

            while (!HookVTable((void*)InterceptRequest, (void**)&Cookie::ProcessRequestOG, false));

            auto eosLib = LoadLibraryW(L"EOSSDK-Win64-Shipping.dll");
            if (eosLib)
            {
                HookVTable((void*)InterceptEOS, (void**)&Cookie::ProcessRequestOG_EOS, true);

                if (Settings.bHasPushWidget)
                {
                    Cookie::PE::ImageBase = *(uint64_t*)(__readgsqword(0x60) + 0x10);

                    const uint8_t pw1[] = { 0x48,0x89,0x5C,0x24,0xCC,0x48,0x89,0x6C,0x24,0xCC,0x48,0x89,0x74,0x24,0xCC,0x57,0x48,0x83,0xEC,0x30,0x48,0x8B,0xE9,0x49,0x8B,0xD9,0x48,0x8D,0x0D };
                    const char   pm1[] = "xxxx?xxxx?xxxx?xxxxxxxxxxxxxx";
                    const uint8_t pw2[] = { 0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0xB8,0x48,0x81,0xEC };
                    const char   pm2[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
                    const uint8_t pw3[] = { 0x48,0x8B,0xC4,0x48,0x89,0x58,0xCC,0x48,0x89,0x70,0xCC,0x48,0x89,0x78,0xCC,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0xA1,0x48,0x81,0xEC };
                    const char   pm3[] = "xxxxxx?xxxx?xxxx?xxxxxxxxxxxx";
                    const uint8_t pw4[] = { 0x48,0x89,0x5C,0x24,0xCC,0x48,0x89,0x74,0x24,0xCC,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24,0xCC,0x48,0x81,0xEC };
                    const char   pm4[] = "xxxx?xxxx?xxxxxxxxxxxxxxx?xxxx";

                    auto hasPW = Cookie::Patch::FindPattern(pw1, pm1, sizeof(pw1))
                        || Cookie::Patch::FindPattern(pw2, pm2, sizeof(pw2))
                        || Cookie::Patch::FindPattern(pw3, pm3, sizeof(pw3))
                        || Cookie::Patch::FindPattern(pw4, pm4, sizeof(pw4));

                    if (hasPW)
                    {
                        const uint8_t exit1[] = { 0x48,0x89,0x5C,0x24,0xCC,0x57,0x48,0x83,0xEC,0x40,0x41,0xB9 };
                        const char   em1[]  = "xxxx?xxxxxxxx";
                        const uint8_t exit2[] = { 0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x88,0x50,0x10,0x88,0x48,0x08,0x57,0x48,0x83,0xEC,0x30 };
                        const char   em2[]  = "xxxxxxxxxxxxxxxxxx";
                        const uint8_t exit3[] = { 0x4C,0x8B,0xDC,0x49,0x89,0x5B,0x08,0x49,0x89,0x6B,0x10,0x49,0x89,0x73,0x18,0x49,0x89,0x7B,0x20,0x41,0x56,0x48,0x83,0xEC,0x30,0x80,0x3D };
                        const char   em3[]  = "xxxxxxxxxxxxxxxxxxxxxxxxxxx";

                        auto exitFunc = Cookie::Patch::FindPattern(exit1, em1, sizeof(exit1));
                        if (!exitFunc) exitFunc = Cookie::Patch::FindPattern(exit2, em2, sizeof(exit2));
                        if (!exitFunc) exitFunc = Cookie::Patch::FindPattern(exit3, em3, sizeof(exit3));

                        const uint8_t sec1[] = { 0x4C,0x8B,0xDC,0x55,0x49,0x8D,0xAB };
                        const char   sm1[]  = "xxxxxxx";
                        const uint8_t sec2[] = { 0x48,0x89,0x5C,0x24,0xCC,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24,0xCC,0x48,0x81,0xEC };
                        const char   sm2[]  = "xxxx?xxxxxxxxxxxxxxxxx?xxxx";
                        const uint8_t sec3[] = { 0x48,0x89,0x5C,0x24,0xCC,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24 };
                        const char   sm3[]  = "xxxx?xxxxxxxxxxxxxxxxxxxx";
                        const uint8_t sec4[] = { 0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24 };
                        const char   sm4[]  = "xxxxxxxxxxxxxxx";

                        auto secMsg = Cookie::Patch::FindPattern(sec1, sm1, sizeof(sec1));
                        if (!secMsg) secMsg = Cookie::Patch::FindPattern(sec2, sm2, sizeof(sec2));
                        if (!secMsg) secMsg = Cookie::Patch::FindPattern(sec3, sm3, sizeof(sec3));
                        if (!secMsg) secMsg = Cookie::Patch::FindPattern(sec4, sm4, sizeof(sec4));

                        if (exitFunc)
                        {
                            DWORD old;
                            VirtualProtect((LPVOID)exitFunc, 1, PAGE_EXECUTE_READWRITE, &old);
                            *(uint8_t*)exitFunc = 0xC3;
                            VirtualProtect((LPVOID)exitFunc, 1, old, &old);
                        }
                        if (secMsg)
                        {
                            DWORD old;
                            VirtualProtect((LPVOID)secMsg, 1, PAGE_EXECUTE_READWRITE, &old);
                            *(uint8_t*)secMsg = 0xC3;
                            VirtualProtect((LPVOID)secMsg, 1, old, &old);
                        }
                    }
                }
            }

            Cookie::Console::Initialize();
        }
    }
}
