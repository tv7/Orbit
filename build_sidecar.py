"""Build the SteamSwitch Python sidecar (server.py) into a standalone binary.

The Tauri app spawns this binary (server.exe on Windows) next to its own
executable. Unlike the old pywebview build, the sidecar has NO heavy native deps
(no pywebview / pythonnet / WebView2) — it's a pure-stdlib HTTP server — so
--onefile is fine here: the extraction is tiny and fast.

Usage (on the target OS; PyInstaller can't cross-compile):

    python build_sidecar.py

Produces:  dist/server.exe   (Windows)
           dist/server       (Linux/macOS)

Copy that next to the built SteamSwitch app executable (see src-tauri/README.md).
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def ensure(pkg):
    try:
        __import__(pkg.split("==")[0].replace("-", "_").lower())
    except ImportError:
        print(f"Installing build dependency: {pkg}")
        subprocess.check_call([sys.executable, "-m", "pip", "install", pkg])


def main():
    ensure("PyInstaller")

    assets = ROOT / "assets"
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--name", "server",
        "--noconfirm",
        "--clean",
        # The sidecar reads the logo from assets/ for the onState payload. It does
        # NOT need web/ in production (Tauri serves the UI); web/ is only used by the
        # sidecar's dev static-file server.
        "--add-data", f"{assets}{os.pathsep}assets",
        str(ROOT / "server.py"),
    ]
    print("Running:", " ".join(cmd))
    subprocess.check_call(cmd, cwd=ROOT)

    out = ROOT / "dist" / ("server.exe" if sys.platform.startswith("win") else "server")
    print("\nDone. Sidecar binary:")
    print("   ", out)
    print("\nCopy it next to the built SteamSwitch executable (see src-tauri/README.md).")


if __name__ == "__main__":
    main()
