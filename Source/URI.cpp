#include "pch.h"
#include "URI.h"
#include "Settings.h"

namespace Cookie
{
    namespace Filter
    {
        static const wchar_t* RedirectedPaths[] =
        {
            L"/fortnite/api/v2/versioncheck/",
            L"/fortnite/api/game/v2/profile/",
            L"/content/api/pages/",
            L"/affiliate/api/public/affiliates/slug",
            L"/socialban/api/public/v1",
            L"/fortnite/api/cloudstorage/system"
        };

        static const wchar_t* RedirectedPathsDev[] =
        {
            L"/fortnite/api/game/v2/profile/",
            L"/affiliate/api/public/affiliates/slug",
            L"/content/api/pages/"
        };

        static const wchar_t* RedirectedDomains[] =
        {
            L"ol.epicgames.com",
            L"ol.epicgames.net",
            L"on.epicgames.com",
            L"game-social.epicgames.com",
            L"ak.epicgames.com",
            L"epicgames.dev",
            L"superawesome.com"
        };

        bool Check(URL& uri)
        {
            switch (Settings.Mode)
            {
            case RedirectLevel::All:
                return true;
            case RedirectLevel::Hybrid:
                for (auto path : RedirectedPaths)
                    if (uri.Path.StartsWith(path))
                        return true;
                break;
            case RedirectLevel::DevOnly:
                for (auto path : RedirectedPathsDev)
                    if (uri.Path.StartsWith(path))
                        return true;
                break;
            default:
                for (auto domain : RedirectedDomains)
                    if (uri.Host.EndsWith(domain))
                        return true;
                break;
            }
            return false;
        }
    }
}
