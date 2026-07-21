# Builds dist\Orbit-portable.zip from build\Release — ONLY what a release needs.
# Strips everything personal or dev-only: data\ (settings, launch history, cover
# cache), startup.log, ssdiag/sstests, debug artifacts. Run from anywhere:
#
#   powershell -ExecutionPolicy Bypass -File src\tools\make-release-zip.ps1
#
# Prerequisites (run once before): the Release build + windeployqt:
#   cmake --build build --config Release
#   windeployqt --release --qmldir src\ui\qml build\Release\Orbit.exe
$ErrorActionPreference = "Stop"

$root    = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent   # repo root
$release = Join-Path $root "build\Release"

if (!(Test-Path (Join-Path $release "Orbit.exe"))) {
    throw "build\Release\Orbit.exe not found - build first: cmake --build build --config Release"
}
if (!(Test-Path (Join-Path $release "Qt6Core.dll"))) {
    throw "Qt DLLs missing - run: windeployqt --release --qmldir src\ui\qml build\Release\Orbit.exe"
}

# Stage a clean copy named Orbit\ (the folder name inside the zip).
$stageRoot = Join-Path $env:TEMP "OrbitReleaseStage"
$stage     = Join-Path $stageRoot "Orbit"
Remove-Item -Recurse -Force $stageRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item "$release\*" $stage -Recurse

# Ship the license inside the portable folder (MIT requires the notice travels with
# the software).
$license = Join-Path $root "LICENSE"
if (Test-Path $license) { Copy-Item $license $stage }

# Personal data + dev-only files that must never ship.
foreach ($junk in "data", "startup.log", "ssdiag.exe", "sstests.exe") {
    Remove-Item -Recurse -Force (Join-Path $stage $junk) -ErrorAction SilentlyContinue
}
Get-ChildItem $stage -Recurse -Include *.pdb, *.ilk, *.lib, *.exp | Remove-Item -Force

if (!(Test-Path (Join-Path $stage "vc_redist.x64.exe"))) {
    Write-Host "note: vc_redist.x64.exe not in build\Release - copy it there first if you want it bundled (v1.0.0 had it)."
}

$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$zip = Join-Path $dist "Orbit-portable.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $stage -DestinationPath $zip

Remove-Item -Recurse -Force $stageRoot

# Emit a SHA-256 so users can verify the download (we're not code-signed yet).
# Publish this value on the GitHub release page next to the zip.
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
"$hash  Orbit-portable.zip" | Out-File -Encoding ascii (Join-Path $dist "Orbit-portable.zip.sha256")
Write-Host "OK -> $zip"
Write-Host "SHA256: $hash"
