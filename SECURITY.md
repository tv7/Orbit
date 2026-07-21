# Security & privacy

ORBIT is a local desktop launcher for Windows. It has no account of its own, no
server, and no telemetry. This document states exactly what it touches so you can
trust it (or read the source — it's all in `src/`, MIT-licensed).

## What ORBIT never does

- **It never sees your passwords.** ORBIT cannot and does not type, read, store, or
  transmit your Steam/Epic/GOG/Xbox password. Account switching works only through
  Steam's *own* saved login token, which exists only after you signed into that
  account once yourself with "Remember me". If a token has expired, ORBIT detects it
  and asks you to sign in manually that one time — it never automates login.
- **No server, no account, no cloud.** Nothing you do is sent anywhere for ORBIT's
  benefit. There is no backend, no login, no analytics, no telemetry, no crash
  reporting.
- **No elevation.** ORBIT runs as your normal user. It writes only to your own
  registry hive (HKCU) and to files it owns.

## What it reads

- Steam: `appmanifest_*.acf`, `loginusers.vdf`, and each account's
  `localconfig.vdf` / `userdata` folders (to list games and resolve which local
  account owns each one).
- Epic (`*.item` manifests), GOG (registry under `HKLM\...\GOG.com\Games`), Xbox/Game
  Pass (`MicrosoftGame.config`) — read-only, to enumerate installed games.
- For a **custom** game you add: the executable you pick, its install folder (to find
  cover-art breadcrumbs like `steam_appid.txt`), and its embedded icon.

## What it writes

- **Account switch (HKCU registry):** `Software\Valve\Steam\AutoLoginUser` and
  `RememberPassword`, and `MostRecent` in `loginusers.vdf` — `loginusers.vdf` is
  backed up to `loginusers.vdf.bak` before the first write.
- **Account picker / offline flag:** `AlwaysShowUserChooser` in `config.vdf`, and the
  `WantsOfflineMode` flag when you launch a game offline.
- **Its own data, next to the exe:** `settings.json` (preferences, launch history,
  the version you chose to skip), `custom_games.json` (games you added), and a cover
  cache under `data/covers/`. Delete these any time.
- **Optional:** if you enable "Run on Windows startup", one `HKCU\...\CurrentVersion\
  Run\ORBIT` value (removed when you turn it off).

## Processes it controls

- Shuts down Steam (`steam.exe -shutdown`, force-killing only if it refuses to close)
  so an account switch takes effect, then restarts it.
- Launches games: the Steam appid, a store's launch URI (Epic/GOG/Xbox), or — for a
  custom game — the exe you added, in its own folder.

## Network access

The only network requests ORBIT makes are:

1. **Cover / hero art** from the stores' public art endpoints (Steam, Epic catalog,
   GOG API, Xbox display catalog) — no API key, no account.
2. **Update check** to `api.github.com/repos/tv7/Orbit/releases/latest` on launch, to
   see whether a newer release exists. It only reads the public release info and never
   downloads or installs anything automatically — you click Download and update by
   hand.

## Verifying your download

Releases are portable zips. Each release lists a **SHA-256 checksum** for the zip;
compare it after downloading:

```powershell
Get-FileHash .\Orbit-portable.zip -Algorithm SHA256
```

The value must match the one on the release page. (ORBIT is not yet code-signed, so
Windows SmartScreen may warn "unknown publisher" — the checksum is how you confirm the
file is the real one until signing is in place.)

## Reporting a vulnerability

Open a private security advisory on the GitHub repo
(**Security → Report a vulnerability**), or open an issue for non-sensitive concerns.
Please don't disclose a security issue publicly until it's been addressed.
