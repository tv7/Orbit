**English** | [العربية](README.ar.md)

# ORBIT

**One launcher for every game on your PC. ORBIT auto-detects everything installed
from Steam, Epic Games, GOG and Xbox Game Pass and puts it all in a single fast,
native library — click a game and it launches through the right store, every time.**

![ORBIT in action: one library for all your stores — click a game and it launches](assets/demo.gif)

Four launchers, four logins, four windows just to see what you own — ORBIT replaces
the daily juggling with one window: your whole library, real cover art, playtime,
and a `Ctrl+K` palette to jump to any game.

- **One library, four stores** — Steam, Epic, GOG and Game Pass installs detected
  automatically. No accounts to connect, no setup.
- **Click and play** — each game launches the way its store expects; you never
  think about *where* a game came from again.
- **Nothing fabricated** — the library shows only what's really on your disk: real
  installs, real owners, real playtime.
- **Multi-account Steam, solved** — got games spread across Steam accounts? ORBIT
  knows which account owns each game and switches to it for you on launch
  ([details below](#steam-extras-account-switching--offline-mode)).
- **Offline mode** — one click to play a Steam game offline, handled correctly.
- **Private by design** — never sees your passwords, no server, no telemetry;
  everything stays local.
- **Native and fast** — a single C++/Qt 6 app. No browser runtime, no background
  services, instant startup.
- **English + Arabic** (full RTL).
- **Windows 10/11** for now.

## ⬇️ Download

**Grab the latest release → [Releases](https://github.com/tv7/Orbit/releases/latest)**

1. Download **`Orbit-portable.zip`**.
2. Unzip it anywhere (Desktop, `D:\Apps\Orbit`, a USB stick — doesn't matter).
3. Run **`Orbit.exe`** inside the folder.

That's it — no installer, no admin rights, nothing to configure. Just keep the
files in the folder together (the Qt runtime ships alongside the exe). Windows
10/11.

**Your passwords are never touched.** ORBIT has no login of its own and never
asks for, sees, stores, or transmits your Steam/Epic/GOG/Xbox passwords — account
switching works through Steam's own "Remember me" logins, the same ones Steam
already keeps on your PC. Nothing is sent to third parties: there's no server, no
telemetry, no analytics. Everything the app knows lives in local files next to it,
and its only network traffic is downloading cover art from the stores' public
image servers.

> Using several Steam accounts? Log into each once with **"Remember me"** checked
> so ORBIT can switch between them. See [First-time setup](#first-time-setup).

---

## The four stores

| Store | How games are found | How they launch |
|-------|--------------------|-----------------|
| **Steam** | `appmanifest_*.acf` in every library folder; owner resolved from Steam's local data (LastOwner → userdata → playtime tiebreak) | Switch to the owning account if needed → Steam launches the appid |
| **Epic Games** | The launcher's install manifests (`%PROGRAMDATA%\Epic\...\Manifests`) | `com.epicgames.launcher://` deep link (silent) |
| **GOG** | Windows registry (`HKLM\...\GOG.com\Games`) — Galaxy not required | GOG Galaxy if installed, else the DRM-free exe directly |
| **Xbox / Game Pass** | `<drive>:\XboxGames` (`MicrosoftGame.config`) | `shell:AppsFolder` AUMID (the only way UWP titles can start) |

Only Steam involves account switching — Epic/GOG/Xbox titles just launch under
whatever their own client is signed into.

## First-time setup

1. Run **`Orbit.exe`**. A one-time setup screen shows which stores were detected —
   there's nothing to connect or configure.
2. Click any game to play. `Ctrl+K` opens the search palette from anywhere.
3. **Using more than one Steam account?** Log into each once with **"Remember
   me"** checked so ORBIT can switch between them (mandatory — see the limits
   below). Games are mapped to their owning account **automatically** from local
   data — no API key needed.

## Steam extras: account switching & offline mode

If all your games live on one account per store, ORBIT is simply a launcher and
you can stop reading here. But if your Steam games are spread across **multiple
accounts**, this is the feature that removes the daily pain: click a game and
ORBIT switches Steam to the account that owns it, waits for the login, and
launches — no logging out and back in by hand, no retyping passwords.

- **Accounts → Add account** restarts Steam to its sign-in screen so you can log
  in (check **"Remember me"**); the new account then appears with a **✓ ready**
  badge once it can be switched to.
- **Accounts → Switch now** switches Steam to an account without launching
  anything.
- A game can occasionally stay **unmapped** (some family-shared installs record no
  local owner) — pin it to an account from its detail page (**Pin to account…**).

### Two hard limits (by Steam's design)

1. **ORBIT never types your password.** Steam blocks scripted password entry (2FA
   by design). Silent switching works because Steam keeps a saved login after you
   log in once with "Remember me". When that saved login expires, ORBIT **detects
   it** and tells you to log in manually that one time — it doesn't pretend to
   automate it. A pre-flight check also warns up front if an account was never
   logged in with "Remember me", before touching your running Steam.

2. **Offline mode uses Steam's `WantsOfflineMode` flag.** Steam has no working
   offline CLI flag, and cold-starting offline with no live session just hangs. So
   ORBIT does what actually works: it logs the account in **online** first (which
   mints the session offline needs), sets `WantsOfflineMode=1` while Steam is
   running, then restarts Steam so it comes up **offline** — and only then launches
   the game. If Steam comes back online instead, it does **not** launch, so a
   shared account is never used online by mistake. Use case: play a shared account
   without syncing over the other player's cloud saves.

## Architecture

ORBIT is a **single native C++17 application**: a dependency-free core (`src/core/`
— pure C++, no Qt: VDF/JSON parsers, the OS platform layer, and one enumerator +
launcher per store behind an `IStore` interface) under a **Qt 6 / QML** UI
(`src/ui/`). The core is fully unit-tested headless against a synthetic Steam tree
— no real Steam needed — and `ssdiag` gives a CLI view of everything the app sees.

<details>
<summary>History: the previous SteamSwitch app (Python + Tauri)</summary>

ORBIT started life as **SteamSwitch**, a Tauri (Rust) shell + Python sidecar +
HTML/CSS UI, Steam-only. The C++ core is a behaviour-faithful port of that app's
Python `core/` (validated byte-identical on real hardware), with Epic/GOG/Xbox
added on top. The legacy stack (`core/`, `server.py`, `src-tauri/`, `web/`) was
removed after v1.0.0 — browse it at the
[`v1.0.0` tag](https://github.com/tv7/Orbit/tree/v1.0.0) if you need it.

</details>

## Build from source

One-time toolchain: **CMake ≥ 3.21**, **MSVC** (C++17), **Qt 6.5+**
(Core/Gui/Quick/Network).

```
cmake -S src -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2022_64
cmake --build build --config Release
```

Package the portable folder (bundles the Qt runtime next to the exe):

```
C:\Qt\6.7.2\msvc2022_64\bin\windeployqt --qmldir src\ui\qml --release build\Release\Orbit.exe
```

Zip the resulting folder → that's the release artifact. Without Qt installed,
CMake still builds the headless targets (`sscore`, `ssdiag`, `sstests`) on any OS —
see [`src/README.md`](src/README.md) for details and the parity checks.

## Diagnostics

```
build/ssdiag accounts    # Steam accounts + game→account mapping, with reasons
build/ssdiag epic|gog|xbox   # what each store enumerator sees
build/ssdiag games       # installed Steam games
ctest --test-dir build   # headless test suite
```

## Troubleshooting

### Offline launch: Steam hangs on the "Connecting…" screen

The most common offline problem, and machine-specific (almost always a **corrupt
Steam cache**, not an ORBIT bug). ORBIT detects the hang, cleans up (clears the
offline flag and closes the stuck Steam so your next start is normal), and does
**not** launch the game. To fix:

1. **Clear the download cache:** Steam → Settings → Downloads → **Clear Download
   Cache**. This alone resolves most cases.
2. **Delete `Steam\appcache`** (if step 1 didn't help): exit Steam fully, delete
   the `appcache` folder in the Steam install dir (it's just a cache — games,
   logins and saves are untouched), start Steam once online, then retry.

Also note Steam's own rule: offline needs a recent online login (ORBIT does this
for you), and Steam caps how long you can stay offline before it wants to
reconnect.

### "Finish login in Steam" / a game won't switch accounts

The saved login for that account expired. Log into it once in the Steam window
with **"Remember me"** checked (password + Steam Guard that one time), then click
the game again.

### An Epic / GOG / Game Pass game won't launch

Those launch through their own store's plumbing — make sure the store's client
(Epic Games Launcher / GOG Galaxy / Xbox app) is installed and signed in, and for
Game Pass that the title's license is still active.

## Privacy & safety

- **No passwords, ever.** ORBIT never asks for, reads, stores, or types a
  password. Switching rides Steam's own saved "Remember me" logins; when one
  expires, ORBIT tells you to log in manually in Steam — it doesn't try to
  automate it.
- **No third parties.** No server, no account system, no telemetry, no
  analytics, no API keys. The only network requests are cover-art downloads
  from the stores' public image CDNs and a once-per-launch update check against
  GitHub's public releases API (it never downloads or installs anything on its
  own — you click Download).
- **Everything is local.** Settings, the cover cache, and the game→account
  mapping are plain local files (`settings.json`, `data/`) you can inspect or
  delete at any time.
- **Careful with Steam's files.** Before the first write, `loginusers.vdf` is
  backed up to `loginusers.vdf.bak`; registry changes are limited to the same
  per-user values Steam itself uses for account selection.
- **Open source.** All of the above is verifiable — this repo is the entire app.
  See [`SECURITY.md`](SECURITY.md) for exactly what ORBIT reads, writes, and sends,
  and how to verify your download.

## License

ORBIT is released under the [MIT License](LICENSE) — free to use, modify, and
redistribute, including in commercial and closed-source work, as long as the
copyright notice is kept.
