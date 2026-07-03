# Cookie Redirect

**Fortnite SSL Bypass / Redirect — made by Akyi**

Universal redirect/SSL bypass for Unreal Engine 4/5 games using native HTTP. Works with OGFN and other Fortnite private servers.

## How it works

Hooks `FCurlHttpRequest::ProcessRequest` via VTable swap. Intercepts outbound HTTP requests and rewrites the URL to point at your backend. Supports both the game module and the EOSSDK module.

## Project structure

```
cookie-redirect/
├── dllentry.cpp        Entry point, thread creation
├── Settings.h          Configuration: backend URL, redirect mode, console
├── Engine.h/.cpp       Unreal engine types: FString, HttpRequest, FMemory
├── URI.h/.cpp          URL parser + redirect filter
├── Scan.h              Memory scanning (pattern + string ref)
├── Patcher.h/.cpp      VTable hooking + pushwidget bypass patches
├── resource.h/.rc      Version info
```

## Configuration

Open `Source/Settings.h`:

```cpp
struct Config
{
    bool ShowWindow;           // console for debugging
    RedirectLevel Mode;        // Normal / Hybrid / DevOnly / Aggressive
    Engine::FString Endpoint;  // backend URL (default http://127.0.0.1:3551)
    bool UseCommandLine;       // read -backend= from CLI
    bool ManualMap;            // EAC manual mapper compat
    bool PatchMemoryLeak;      // fix memory leak after 5 reqs
};
```

### Redirect modes

| Mode | Behaviour |
|------|-----------|
| **Normal** | Intercepts Epic Games domains (`ol.epicgames.com`, etc.) |
| **Hybrid** | Only redirects specific paths (profile, version, content, cloudstorage) |
| **DevOnly** | Like Hybrid but fewer redirect paths |
| **Aggressive** | Every HTTP request goes to your backend |

## Building

1. Open `cookie-redirect.sln` in Visual Studio 2022
2. Build **Release x64**
3. Inject `cookie-redirect.dll` into `FortniteClient-Win64-Shipping.exe` aka use a launcher that has injections like rebootlauncher

## Credits

Cookie Redirect — made by Akyi

Discord: akyii16
