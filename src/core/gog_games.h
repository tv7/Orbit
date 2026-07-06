// GOG enumeration + launch. GOG games are DRM-free, and the GOG installers /
// GOG Galaxy record every installed game in the Windows registry under
//   HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<gameID>   (64-bit; GOG installers are 32-bit)
//   HKLM\SOFTWARE\GOG.com\Games\<gameID>               (32-bit fallback)
// so we enumerate from there — no Galaxy SQLite DB dependency. Launch prefers GOG
// Galaxy's `runGame` command (overlay/cloud saves) and falls back to running the
// game's own exe directly (works even with Galaxy uninstalled).
//
// Registry-only, so it's empty on non-Windows; the pure argv/Game builders are
// unit-tested from synthetic registry entries.

#pragma once

#include "model.h"

#include <optional>
#include <string>
#include <vector>

namespace ss::gog {

// The values we read from a game's registry key (all optional in practice).
struct Entry {
    std::string id;             // gameID (also the registry subkey name)
    std::string name;           // gameName
    std::string path;           // install directory
    std::string launchCommand;  // full path to the launch exe
    std::string launchParam;    // launch arguments
    std::string workingDir;     // working directory for the exe
    std::string exe;            // fallback exe path if launchCommand is missing
};

// Installed GOG games (registry; empty on POSIX / when GOG isn't installed).
std::vector<Game> installedGames();

// Read one game's registry entry by id; nullopt if not present.
std::optional<Entry> entry(const std::string& gameId);

// Path to GalaxyClient.exe if GOG Galaxy is installed, else nullopt.
std::optional<std::string> galaxyClientExe();

// ---- pure builders (unit-testable, no registry) ----------------------------
Game toGame(const Entry& e);
// GalaxyClient.exe /command=runGame /gameId=<id> /path=<installdir>
std::vector<std::string> galaxyRunGameArgv(const std::string& galaxyExe, const Entry& e);
// Direct DRM-free launch: launchCommand (or exe) + any launchParam tokens.
std::vector<std::string> directLaunchArgv(const Entry& e);
// The cwd to launch in (workingDir if set, else the install path).
std::string launchWorkingDir(const Entry& e);

// Launch a game by id (Galaxy runGame if Galaxy present, else the exe directly).
PlayResult launch(const std::string& gameId, const Notify& notify = {});

}  // namespace ss::gog
