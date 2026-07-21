// Custom games — user-authored library entries for anything no store detects.
// Unlike the four real stores (which enumerate from the OS), Custom is a list the
// user maintains by hand: pick a game .exe and it appears in the library; ORBIT
// runs that exe directly on Play (like GOG's DRM-free direct-launch path).
//
// The list is persisted to custom_games.json in the app data dir (sibling of
// settings.json). This is the one store that also works off-Windows — launching an
// exe needs no registry/store, so the pure builders + launch are unit-tested and
// exercised on any platform.

#pragma once

#include "model.h"

#include <optional>
#include <string>
#include <vector>

namespace ss::custom {

// One user-added game. `id` is a stable slug generated on add (identity for the
// synthetic UI id + launch history; never reused). `exe` is the absolute path to
// the game executable; `args` are extra command-line tokens; `workingDir` is the
// cwd to launch in (defaults to the exe's folder); `coverPath` is an optional
// local image the user picked for the tile.
struct Entry {
    std::string id;
    std::string name;
    std::string exe;
    std::string args;
    std::string workingDir;
    std::string coverPath;
};

// The saved list (empty + never throws on a missing/corrupt file).
std::vector<Entry> load();

// Look one entry up by id.
std::optional<Entry> find(const std::string& id);

// Add a game. Generates + returns a unique id; `workingDir` defaults to the exe's
// parent folder when empty. Returns "" if `exe` is empty.
std::string add(const std::string& name, const std::string& exe,
                const std::string& args = "", const std::string& coverPath = "");

// Update an existing entry in place (matched by `e.id`). True if it existed.
bool update(const Entry& e);

// Remove an entry by id. True if it existed.
bool remove(const std::string& id);

// Installed games for the library (one Game per saved entry).
std::vector<Game> installedGames();

// Launch a game by id (runs its exe directly, in its working dir).
PlayResult launch(const std::string& id, const Notify& notify = {});

// ---- pure builders (unit-testable, no disk) ---------------------------------
Game toGame(const Entry& e);
// The argv to spawn: exe + whitespace-split args.
std::vector<std::string> launchArgv(const Entry& e);
// The cwd to launch in (workingDir if set, else the exe's parent folder).
std::string launchWorkingDir(const Entry& e);
// Slugify a display name into an id token ("My Game!" -> "my-game"); "game" if empty.
std::string slugify(const std::string& name);

}  // namespace ss::custom
