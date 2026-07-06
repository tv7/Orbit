// Epic Games Launcher enumeration + launch. Epic has no account switching — the
// launcher owns one signed-in account — so this is a pure enumerate + fire-URI
// store (the counterpart of steam_games.h, minus the account/switch machinery).
//
// Installed games are described by per-game JSON manifests the launcher writes to
//   %PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests\*.item
// (override the directory with $SS_EPIC_MANIFESTS for tests / non-default installs).

#pragma once

#include "model.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ss::epic {

namespace fs = std::filesystem;

// The directory holding the *.item manifests ($SS_EPIC_MANIFESTS override →
// %PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests). May not exist if Epic
// isn't installed; callers treat a missing/empty dir as "no Epic games".
fs::path manifestsDir();

// Installed Epic games (base games only — DLC/addon manifests are filtered out).
// Each Game has store=Epic, launchId=AppName, appid=0 (Epic has no numeric id).
std::vector<Game> installedGames();

// The com.epicgames.launcher:// URI that starts a game by its AppName.
std::string launchUri(const std::string& appName);

}  // namespace ss::epic
