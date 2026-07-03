#pragma once
#include "Engine.h"

namespace Cookie
{
    enum RedirectLevel
    {
        Normal,   // redirect epic domains to private server
        Hybrid,   // redirect profile, versioncheck, content
        DevOnly,  // redirect profile, affiliate, content
        All       // redirect every request
    };

    struct Config
    {
        bool ShowWindow        = false;
        bool ConsoleEnabled    = true;   // enable UE4 in-game console (tilde key)
        bool RemoteConsole     = false;  // enable TCP remote command server
        int  RemotePort        = 13337;
        RedirectLevel Mode     = All;
        bool bHasPushWidget    = false;  // patch pushwidget for EOS builds
        bool UseCommandLine    = true;  // read -backend=XXXX from CLI
        bool ManualMapping     = false;  // EAC + manual mapper compat
        bool PatchMemoryLeak   = true;
        const wchar_t* BackendURL = L"http://26.185.228.101:3551";

        FString Backend;
    };

    inline Config Settings;
}
