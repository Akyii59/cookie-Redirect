#include "Framework.h"
#include <vector>

using namespace SDK;

static const wchar_t* kWidgetPaths[] = {
    L"/Game/UI/Competitive/Arena/ArenaScoringHUD.ArenaScoringHUD_C",
    L"/Game/UI/Frontend/Showdown/ShowdownScoringHUD.ShowdownScoringHUD_C",
    L"/Game/UI/Competitive/Arena/ArenaScoringHUD_Primary.ArenaScoringHUD_Primary_C",
};

// Find ALL UFortPlaylistAthena instances in GObjects
static void FindAllPlaylists(std::vector<UFortPlaylistAthena*>& outList)
{
    auto& gobj = UObject::GObjects;
    for (int32 i = 0; i < gobj->Num(); i++)
    {
        auto obj = gobj->GetByIndex(i);
        if (!obj) continue;
        auto shortName = obj->GetName();
        auto fullName = obj->GetFullName();
        if (fullName.find("Class ") == 0) continue;
        if (shortName.find("Playlist_Showdown") != UEAllocatedString::npos ||
            shortName.find("Playlist_Arena") != UEAllocatedString::npos ||
            shortName.find("Playlist_Competitive") != UEAllocatedString::npos ||
            fullName.find("/Game/Athena/Playlists/Showdown/") != UEAllocatedString::npos ||
            fullName.find("/Game/Athena/Playlists/Arena/") != UEAllocatedString::npos)
        {
            outList.push_back((UFortPlaylistAthena*)obj);
        }
    }
}

static void AddUIExtensionToPlaylist(UFortPlaylistAthena* playlist)
{
    auto* Helper = reinterpret_cast<FFortPlaylistAthena_Helper*>(playlist);

    for (auto path : kWidgetPaths)
    {
        // Check if this path already exists in the array
        bool alreadyExists = false;
        auto newExtName = UKismetStringLibrary::Conv_StringToName(path);
        for (int32 i = 0; i < Helper->UIExtensions.Num(); i++)
        {
            auto& ext = Helper->UIExtensions[i];
            if (ext.WidgetClass.ObjectID.AssetPathName.ComparisonIndex == newExtName.ComparisonIndex &&
                ext.WidgetClass.ObjectID.AssetPathName.Number == newExtName.Number)
            {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) continue;

        FUIExtension ArenaUIExtension{};
        ArenaUIExtension.Slot = EUIExtensionSlot::Primary;
        ArenaUIExtension.WidgetClass.ObjectID.AssetPathName =
            UKismetStringLibrary::Conv_StringToName(path);
        Helper->UIExtensions.Add(ArenaUIExtension);
    }
}

DWORD WINAPI ArenaMain(LPVOID)
{
    // Keep retrying to find playlists and add extensions
    for (int tries = 0; tries < 300; tries++)
    {
        std::vector<UFortPlaylistAthena*> playlists;
        FindAllPlaylists(playlists);

        if (!playlists.empty())
        {
            for (auto* pl : playlists)
                AddUIExtensionToPlaylist(pl);

            // Even if we found and added, keep running in case new playlists load
            // Check every 5 seconds instead of 500ms after first success
            tries = 0; // reset timer
            Sleep(5000);
            continue;
        }

        Sleep(500);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, ArenaMain, nullptr, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
