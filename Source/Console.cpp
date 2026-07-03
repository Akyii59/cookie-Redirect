#include "pch.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Console.h"
#include "Settings.h"
#include "Scan.h"

namespace Cookie::Console
{
    // Crystal-19.10 UObject: VTable(0x00) Flags(0x08) Index(0x0C) Class(0x10) Name(0x18) Outer(0x20)
    struct UObject
    {
        void**       VTable;
        uint32_t     Flags;
        uint32_t     Index;
        void*        Class;
        uint64_t     Name;
        UObject*     Outer;
    };

    // UEngine: ConsoleClass at 0xF0, GameViewport at 0x7A0
    struct UEngine : UObject
    {
        uint8_t      Pad_28[0xC8];
        void*        ConsoleClass;    // 0x00F0 TSubclassOf<UConsole>
        uint8_t      Pad_F8[0x6A8];
        void*        GameViewport;    // 0x07A0 UGameViewportClient*
    };

    // UGameViewportClient: ViewportConsole at 0x40
    struct UGameViewportClient : UObject
    {
        uint8_t      Pad_28[0x18];
        void*        ViewportConsole; // 0x0040 UConsole*
    };

    // UConsole with needed fields
    struct UConsole : UObject
    {
        uint8_t      Pad_28[0x10];    // 0x28
        void*        ConsoleTargetPlayer; // 0x38
        void*        DefaultTexture_Black; // 0x40
        void*        DefaultTexture_White; // 0x48
    };

    // GObjects pointer (RVA from module base)
    constexpr uint32_t GObjects_RVA = 0x0B3C9A78;

    // Element count per chunk for TUObjectArray
    constexpr int32_t ElementsPerChunk = 0x10000;

    // Chunked object array element
    struct FUObjectItem
    {
        UObject* Object;        // 0x00
        uint32_t Flags;         // 0x08
        uint32_t ClusterIndex;  // 0x0C
        uint32_t SerialNumber;  // 0x10
    };

    // TUObjectArray chunked container
    struct TUObjectArray
    {
        FUObjectItem** Objects;     // 0x00
        uint8_t        Pad_8[0x8]; // 0x08
        int32_t        MaxElements; // 0x10
        int32_t        NumElements; // 0x14
        int32_t        MaxChunks;   // 0x18
        int32_t        NumChunks;   // 0x1C

        int32_t Num() const { return NumElements; }

        FUObjectItem* GetByIndex(int32_t Index) const
        {
            int32_t ChunkIdx = Index / ElementsPerChunk;
            if (ChunkIdx >= NumChunks || Index >= NumElements)
                return nullptr;
            if (!Objects) return nullptr;
            auto Chunk = Objects[ChunkIdx];
            if (!Chunk) return nullptr;
            auto Item = Chunk + (Index % ElementsPerChunk);
            return Item;
        }
    };

    static UEngine* GEnginePtr = nullptr;
    static bool bConsoleCreated = false;
    static UConsole* ActiveConsole = nullptr;
    static HHOOK g_KbHook = nullptr;
    static HWND g_GameWnd = nullptr;

    // StaticConstructObject_Internal signature
    typedef UObject* (__fastcall* f_SCOI)(
        void* Class, UObject* Outer, void* Name, uint32_t SetFlags,
        uint32_t InternalSetFlags, UObject* Template,
        bool bCopyTransients, void* InstanceGraph, bool bAssumeArchetype);

    static f_SCOI StaticConstructObject_Internal = nullptr;

    static TUObjectArray* GetGObjects()
    {
        return (TUObjectArray*)(Cookie::PE::ImageBase + GObjects_RVA);
    }

    static UEngine* FindEngineViaObjects()
    {
        auto ObjArr = GetGObjects();
        if (!ObjArr) return nullptr;

        auto num = ObjArr->Num();
        for (int32_t i = 0; i < num; i++)
        {
            auto Item = ObjArr->GetByIndex(i);
            if (!Item || !Item->Object || !Item->Object->Class)
                continue;

            void* objClass = Item->Object->Class;
            auto candidate = (UEngine*)Item->Object;

            if (candidate->ConsoleClass && candidate->GameViewport)
                return candidate;
        }
        return nullptr;
    }

    static UEngine* FindEngineViaPattern()
    {
        const uint8_t sig[]  = { 0x48,0x89,0x74,0x24,0x20,0xE8,0x00,0x00,0x00,0x00,0x48,0x8B,0x4C,0x24,0x40,0x48,0x89,0x05 };
        const char    mask[] = "xxxxxx????xxxxxxxx";

        auto addr = Cookie::Patch::FindPattern(sig, mask, sizeof(sig));
        if (!addr) return nullptr;

        int32_t disp = *(int32_t*)(addr + 18);
        auto gEngineAddr = addr + 22 + disp;
        return *(UEngine**)gEngineAddr;
    }

    // Find UConsole class in GObjects by name
    static void* FindConsoleClass()
    {
        auto ObjArr = GetGObjects();
        if (!ObjArr) return nullptr;

        for (int32_t i = 0; i < ObjArr->Num(); i++)
        {
            auto item = ObjArr->GetByIndex(i);
            if (!item || !item->Object || !item->Object->Class)
                continue;

            auto obj = item->Object;
            // Look for UClass with name "Console" in the Engine package
            // The class name is stored in the FName (uint64) at offset 0x18
            // Name is stored as a pair of int32: {ComparisonIndex, Number}
            // We can compare the ComparisonIndex or check via Class pointer

            // Quick heuristic: check if this is a UClass (Class->Class should point to UClass's own class)
            // Then check if its name matches "Console"
            // Read the FName at offset 0x18 - it's {Index, Number}
            auto nameIdx = *(int32_t*)((uint8_t*)obj + 0x18);
            if (nameIdx == 0) continue;

            // We need to resolve the name. Use the SDK's FName::AppendString approach.
            // For now, use a simpler check: compare the class vtable pattern
            // The console class has a specific default object that's a UConsole
            auto classPtr = obj;
            auto defaultObj = *(void**)((uint8_t*)classPtr + 0x50); // UClass::DefaultObject at offset 0x50 typically
            // Hmm, this offset is wrong for our struct. Let me use a different approach.

            // Better: look for UClass that matches the Engine->ConsoleClass
            if (GEnginePtr && GEnginePtr->ConsoleClass == obj)
                return obj;
        }
        return nullptr;
    }

    static void* FindConsoleClassFromEngine()
    {
        if (!GEnginePtr || !GEnginePtr->ConsoleClass)
            return nullptr;
        return GEnginePtr->ConsoleClass;
    }

    static UObject* GetConsoleClassDefaultObject(void* consoleClass)
    {
        if (!consoleClass) return nullptr;
        // UClass::DefaultObject is at offset 0x50 in the UClass structure
        // UClass(0x00) extends UStruct(0x28) extends UField(0x28) extends UObject(0x00)
        // UStruct adds Super(0x28), Children(0x30), ChildProperties(0x38), Size(0x40), MinAlign(0x44)
        // UClass adds CastFlags(0x48), DefaultObject(0x50)
        return *(UObject**)((uint8_t*)consoleClass + 0x50);
    }

    // UE4.27 StaticConstructObject_Internal pattern (common prologue)
    static f_SCOI FindSCOI_UE427()
    {
        // Pattern 1: Standard UE4.27 prologue
        const uint8_t sig1[] = { 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0xF1 };
        const char mask1[] = "xxxxxxxxxxxxxxxxxxxxxxx";
        auto addr = Cookie::Patch::FindPattern(sig1, mask1, sizeof(sig1));
        if (addr) return (f_SCOI)addr;

        // Pattern 2: Alternative UE4.27 prologue
        const uint8_t sig2[] = { 0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0xF1 };
        const char mask2[] = "xxxxxxxxxxxxxxxxxx";
        addr = Cookie::Patch::FindPattern(sig2, mask2, sizeof(sig2));
        if (addr) return (f_SCOI)addr;

        // Pattern 3: UE4 FortConsole SCOI (original)
        const uint8_t sig3[] = { 0x4C,0x89,0x44,0x24,0x18,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24,0x00,0x00,0x00,0x00,0x48,0x81,0xEC,0x00,0x00,0x00,0x00,0x48,0x8B,0x05,0x00,0x00,0x00,0x00,0x48,0x33,0xC4 };
        const char mask3[] = "xxxxxxxxxxxxxxxxxxxxx????xxx????xxx????xxx";
        addr = Cookie::Patch::FindPattern(sig3, mask3, sizeof(sig3));
        return (f_SCOI)addr;
    }

    // Toggle console visibility by calling UConsole::ShowConsole vtable function
    static void ToggleConsole()
    {
        if (!GEnginePtr) return;
        auto viewport = (UGameViewportClient*)GEnginePtr->GameViewport;
        if (!viewport || !viewport->ViewportConsole) return;

        auto consoleObj = (UConsole*)viewport->ViewportConsole;
        auto vtable = consoleObj->VTable;
        if (!vtable) return;

        // Try to find and call ShowConsole() via vtable
        // UConsole vtable: UObject has ~8 entries, then UConsole entries
        // ShowConsole is typically at index 0x08-0x0C (first few UConsole virtuals)
        // For UE4.27, try indices 7-15 to find it safely
        for (int idx = 7; idx <= 15; idx++)
        {
            if (!vtable[idx]) continue;
            void* func = vtable[idx];
            if (!func) continue;

            __try
            {
                ((void(*)(void*))func)(consoleObj);
                return;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }

        // Fallback: try using GEngine vtable for Exec function
        uint64_t cmdBuf[] = { 0, 0, 0 };
        auto* cmdStr = (wchar_t*)L"showconsole";
        // Try to call GEngine->Exec if we can find it
        // GEngine vtable: UObject(8) + UEngine virtuals, Exec is usually at 0x18-0x22
        auto engineVt = GEnginePtr->VTable;
        if (!engineVt) return;

        for (int idx = 0x18; idx <= 0x28; idx++)
        {
            if (!engineVt[idx]) continue;
            __try
            {
                using ExecFunc = bool(__fastcall*)(void*, void*, const wchar_t*, void*);
                auto execFn = (ExecFunc)engineVt[idx];
                if (execFn(GEnginePtr, nullptr, cmdStr, &cmdBuf))
                    return;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                continue;
            }
        }
    }

    static LRESULT CALLBACK KbProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
        {
            auto kbd = (KBDLLHOOKSTRUCT*)lParam;
            if (kbd->vkCode == VK_F8 || kbd->vkCode == VK_OEM_3)
            {
                ToggleConsole();
                return 1;
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    static void StartRemoteServer()
    {
        auto port = (uint16_t)Settings.RemotePort;
        if (port == 0) port = 13337;

        WSADATA wsa = { 0 };
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

        auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) { WSACleanup(); return; }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        { closesocket(sock); WSACleanup(); return; }

        listen(sock, 1);

        char buf[1024];

        while (true)
        {
            struct sockaddr_in client;
            int cliSize = sizeof(client);
            auto clientSock = accept(sock, (struct sockaddr*)&client, &cliSize);
            if (clientSock == INVALID_SOCKET) continue;

            int bytes = recv(clientSock, buf, sizeof(buf) - 1, 0);
            if (bytes > 0)
            {
                buf[bytes] = 0;

                while (bytes > 0 && (buf[bytes - 1] == '\n' || buf[bytes - 1] == '\r'))
                    buf[--bytes] = 0;

                if (strlen(buf) > 0)
                {
                    if (Settings.ShowWindow)
                        printf("[RemoteConsole] %s\n", buf);

                    wchar_t wbuf[1024];
                    size_t conv = 0;
                    mbstowcs_s(&conv, wbuf, buf, _TRUNCATE);

                    // Execute via GEngine->Exec
                    if (GEnginePtr)
                    {
                        uint64_t dummyOut[3] = {};
                        auto vt = GEnginePtr->VTable;
                        if (vt)
                        {
                            for (int idx = 0x18; idx <= 0x28; idx++)
                            {
                                if (!vt[idx]) continue;
                                __try
                                {
                                    using ExecFunc = bool(__fastcall*)(void*, void*, const wchar_t*, void*);
                                    auto execFn = (ExecFunc)vt[idx];
                                    execFn(GEnginePtr, nullptr, wbuf, &dummyOut);
                                    break;
                                }
                                __except (EXCEPTION_EXECUTE_HANDLER)
                                {
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
            closesocket(clientSock);
        }
    }

    void Initialize()
    {
        if (!Settings.ConsoleEnabled)
            return;

        // Phase 1: Find GEngine
        GEnginePtr = FindEngineViaPattern();
        if (!GEnginePtr)
            GEnginePtr = FindEngineViaObjects();

        if (!GEnginePtr)
        {
            if (Settings.ShowWindow)
                printf("[Console] Failed to find GEngine\n");
            return;
        }

        if (Settings.ShowWindow)
            printf("[Console] GEngine: %p\n", GEnginePtr);

        // Phase 2: Find StaticConstructObject_Internal (try UE4.27 patterns)
        StaticConstructObject_Internal = FindSCOI_UE427();

        // Phase 3: Find ConsoleClass from GEngine
        auto consoleClass = FindConsoleClassFromEngine();
        if (!consoleClass)
        {
            if (Settings.ShowWindow)
                printf("[Console] GEngine->ConsoleClass is null\n");
            return;
        }

        // Phase 4: Wait for GameViewport to exist
        UGameViewportClient* viewport = nullptr;
        for (int tries = 0; tries < 600; tries++)
        {
            if (GEnginePtr)
            {
                viewport = (UGameViewportClient*)GEnginePtr->GameViewport;
                if (viewport && !viewport->ViewportConsole)
                    break;
            }
            Sleep(500);
        }

        if (!viewport)
        {
            if (Settings.ShowWindow)
                printf("[Console] GameViewport not found\n");
            return;
        }

        // Phase 5: Create UConsole or use CDO fallback
        UObject* consoleObj = nullptr;

        if (StaticConstructObject_Internal)
        {
            if (Settings.ShowWindow)
                printf("[Console] Using SCOI to create UConsole\n");

            consoleObj = StaticConstructObject_Internal(
                consoleClass, (UObject*)viewport, nullptr,
                0, 0, nullptr, false, nullptr, false
            );
        }

        if (!consoleObj)
        {
            if (Settings.ShowWindow)
                printf("[Console] SCOI not available, using CDO fallback\n");

            consoleObj = GetConsoleClassDefaultObject(consoleClass);
            if (consoleObj && Settings.ShowWindow)
                printf("[Console] Using CDO: %p\n", consoleObj);
        }

        if (!consoleObj)
        {
            if (Settings.ShowWindow)
                printf("[Console] Failed to get/create UConsole\n");
            return;
        }

        viewport->ViewportConsole = consoleObj;
        ActiveConsole = (UConsole*)consoleObj;
        bConsoleCreated = true;

        if (Settings.ShowWindow)
            printf("[Console] Console created and attached\n");

        // Phase 6: Install keyboard hook for F8 and tilde
        g_KbHook = SetWindowsHookEx(WH_KEYBOARD_LL, KbProc, GetModuleHandleW(nullptr), 0);
        g_GameWnd = FindWindowW(L"UnrealWindow", L"Fortnite  ");

        if (Settings.ShowWindow)
            printf("[Console] KbHook: %s, GameWnd: %p\n",
                g_KbHook ? "OK" : "FAIL", g_GameWnd);

        // Phase 7: Start remote command server if enabled
        if (Settings.RemoteConsole)
        {
            CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                StartRemoteServer();
                return 0;
            }, nullptr, 0, nullptr);
        }
    }
}
