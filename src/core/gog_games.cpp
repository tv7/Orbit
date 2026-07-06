#include "gog_games.h"

#include "platform.h"

#include <algorithm>
#include <sstream>

namespace ss::gog {

namespace {

using platform::Hive;

// Registry roots the GOG installers write to (64-bit WOW node first).
const char* kGameRoots[] = {
    "SOFTWARE\\WOW6432Node\\GOG.com\\Games",
    "SOFTWARE\\GOG.com\\Games",
};
const char* kGalaxyPathRoots[] = {
    "SOFTWARE\\WOW6432Node\\GOG.com\\GalaxyClient\\paths",
    "SOFTWARE\\GOG.com\\GalaxyClient\\paths",
};

std::string reg(const std::string& subkey, const std::string& name) {
    // Registry value names are case-insensitive, so the stored casing doesn't matter.
    return platform::regReadString(Hive::LocalMachine, subkey, name).value_or("");
}

// Split a launchParam string into argv tokens on whitespace (GOG params are simple;
// most games have none). Quoted params aren't common here — kept deliberately plain.
std::vector<std::string> splitParams(const std::string& params) {
    std::vector<std::string> out;
    std::istringstream in(params);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

}  // namespace

std::optional<Entry> entry(const std::string& gameId) {
    for (const char* root : kGameRoots) {
        std::string key = std::string(root) + "\\" + gameId;
        std::string name = reg(key, "gameName");
        std::string path = reg(key, "path");
        if (name.empty() && path.empty()) continue;   // key not present under this root
        Entry e;
        e.id = gameId;
        e.name = name.empty() ? gameId : name;
        e.path = path;
        e.launchCommand = reg(key, "launchCommand");
        e.launchParam = reg(key, "launchParam");
        e.workingDir = reg(key, "workingDir");
        e.exe = reg(key, "exe");
        return e;
    }
    return std::nullopt;
}

std::vector<Game> installedGames() {
    std::vector<Game> out;
    std::vector<std::string> seen;
    for (const char* root : kGameRoots) {
        for (const std::string& id : platform::regSubKeys(Hive::LocalMachine, root)) {
            if (std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
            auto e = entry(id);
            if (!e) continue;
            seen.push_back(id);
            out.push_back(toGame(*e));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const Game& a, const Game& b) { return a.name < b.name; });
    return out;
}

std::optional<std::string> galaxyClientExe() {
    for (const char* root : kGalaxyPathRoots) {
        std::string dir = reg(root, "client");
        if (!dir.empty()) return dir + "\\GalaxyClient.exe";
    }
    return std::nullopt;
}

Game toGame(const Entry& e) {
    Game g;
    g.store = Store::Gog;
    g.appid = 0;                 // GOG has no Steam-style numeric appid; identity is launchId
    g.launchId = e.id;
    g.name = e.name;
    g.installdir = e.path;
    g.fullyInstalled = true;     // a registry entry means the game is installed
    return g;
}

std::vector<std::string> galaxyRunGameArgv(const std::string& galaxyExe, const Entry& e) {
    std::vector<std::string> argv = {galaxyExe, "/command=runGame", "/gameId=" + e.id};
    if (!e.path.empty()) argv.push_back("/path=" + e.path);
    return argv;
}

std::vector<std::string> directLaunchArgv(const Entry& e) {
    std::string exe = !e.launchCommand.empty() ? e.launchCommand : e.exe;
    if (exe.empty()) return {};
    std::vector<std::string> argv = {exe};
    for (auto& p : splitParams(e.launchParam)) argv.push_back(p);
    return argv;
}

std::string launchWorkingDir(const Entry& e) {
    return !e.workingDir.empty() ? e.workingDir : e.path;
}

PlayResult launch(const std::string& gameId, const Notify& notify) {
    auto e = entry(gameId);
    if (!e) return PlayResult::fail("That GOG game is no longer installed.");
    if (notify) notify("Launching \"" + e->name + "\"…");

    // Prefer GOG Galaxy (gets its overlay / cloud saves) when it's installed.
    if (auto galaxy = galaxyClientExe()) {
        platform::spawnDetached(galaxyRunGameArgv(*galaxy, *e));
        return PlayResult::success("Handed \"" + e->name + "\" to GOG Galaxy.");
    }

    // Otherwise run the DRM-free exe directly, in its install dir.
    auto argv = directLaunchArgv(*e);
    if (argv.empty())
        return PlayResult::fail("Couldn't find the launch command for \"" + e->name + "\".");
    platform::spawnDetached(argv, launchWorkingDir(*e));
    return PlayResult::success("Launched \"" + e->name + "\".");
}

}  // namespace ss::gog
