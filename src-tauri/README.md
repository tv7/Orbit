# SteamSwitch — Tauri shell

A thin native host (Rust) for the existing `web/` UI. It opens the window
(WebView2 initialises **asynchronously**, so no "Not Responding" freeze), spawns
the Python sidecar (`server.py`), reads the sidecar's `STEAMSWITCH_SIDECAR_READY
<url>` line, injects that URL as `window.__SIDECAR__`, and kills the sidecar on
close. All app logic stays in `core/*` behind the sidecar — there is no business
logic in Rust.

```
src-tauri/src/main.rs   spawn sidecar + open window + inject URL + cleanup
        tauri.conf.json  window/bundle config (frontendDist = ../web)
        capabilities/    Tauri v2 permissions (core only; no plugins)
../server.py             Python sidecar (HTTP + SSE over core/*)
../web/bridge.js         shim: window.pywebview.api.* -> HTTP, SSE -> window.on*
```

## One-time toolchain (build machine only — end users need none)

1. **Rust** — https://rustup.rs (`rustup` installs `cargo`).
2. **Tauri CLI** — `cargo install tauri-cli --version "^2"` (gives `cargo tauri`).
3. **WebView2 runtime** — already on Windows 10/11.
4. **Python + PyInstaller** — for building the sidecar (`pip install pyinstaller`).
5. **App icons** (first time): from the repo root run
   `cargo tauri icon assets/steamswitch.png`
   — this generates `src-tauri/icons/*` referenced by `tauri.conf.json`.

## Develop (fast iteration)

From the repo root:

```
cargo tauri dev
```

In dev the shell runs `python server.py` from the repo root automatically, so you
just need Python + the `core/` deps (stdlib only). Edit `web/*` and reload.

## Build a release

```
python build_sidecar.py          # -> dist/server.exe
cargo tauri build                # -> src-tauri/target/release/bundle/...
```

Then place the sidecar next to the app executable so `main.rs` (release) can find
it:

```
copy dist\server.exe  <the folder containing SteamSwitch.exe>
```

(For a polished installer, wire `server.exe` in as a Tauri **resource** or
**externalBin** so it's bundled automatically — a follow-up; the copy step works
for now.)

## Notes

- The sidecar binds `127.0.0.1` on an OS-assigned free port (`--port 0`) and prints
  the real URL; the shell reads it. No fixed port to clash on.
- CSP is disabled (`app.security.csp = null`) because the UI talks to a localhost
  origin; acceptable for a local desktop app.
