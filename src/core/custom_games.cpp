#include "custom_games.h"

#include "appdata.h"
#include "json.h"
#include "platform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>

namespace ss::custom {

namespace fs = std::filesystem;

namespace {

fs::path storeFile() { return appdata::dir() / "custom_games.json"; }

// Load the games array (empty on any error — parity with settings.cpp's load()).
std::vector<Entry> loadEntries() {
    std::vector<Entry> out;
    std::error_code ec;
    if (!fs::exists(storeFile(), ec)) return out;
    std::ifstream f(storeFile(), std::ios::binary);
    if (!f) return out;
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    bool ok = false;
    json::Value v = json::parse(text, &ok);
    if (!ok || !v.isObject()) return out;
    const json::Value* games = v.get("games");
    if (!games || !games->isArray()) return out;
    for (const json::Value& g : games->arr) {
        if (!g.isObject()) continue;
        Entry e;
        e.id = g.get("id") ? g.get("id")->asString() : "";
        e.name = g.get("name") ? g.get("name")->asString() : "";
        e.exe = g.get("exe") ? g.get("exe")->asString() : "";
        e.args = g.get("args") ? g.get("args")->asString() : "";
        e.workingDir = g.get("workingDir") ? g.get("workingDir")->asString() : "";
        e.coverPath = g.get("coverPath") ? g.get("coverPath")->asString() : "";
        if (!e.id.empty()) out.push_back(std::move(e));
    }
    return out;
}

void saveEntries(const std::vector<Entry>& entries) {
    json::Value games = json::Value::makeArray();
    for (const Entry& e : entries) {
        json::Value g = json::Value::makeObject();
        g.set("id", json::Value::makeString(e.id));
        g.set("name", json::Value::makeString(e.name));
        g.set("exe", json::Value::makeString(e.exe));
        g.set("args", json::Value::makeString(e.args));
        g.set("workingDir", json::Value::makeString(e.workingDir));
        g.set("coverPath", json::Value::makeString(e.coverPath));
        games.arr.push_back(std::move(g));
    }
    json::Value root = json::Value::makeObject();
    root.set("games", std::move(games));

    std::error_code ec;
    fs::create_directories(appdata::dir(), ec);
    std::ofstream f(storeFile(), std::ios::binary | std::ios::trunc);
    if (f) f << json::dump(root, 2);   // best-effort like settings.cpp
}

}  // namespace

std::string slugify(const std::string& name) {
    std::string out;
    bool lastDash = false;
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            out.push_back((char)std::tolower(c));
            lastDash = false;
        } else if (!out.empty() && !lastDash) {
            out.push_back('-');
            lastDash = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "game" : out;
}

std::vector<Entry> load() { return loadEntries(); }

std::optional<Entry> find(const std::string& id) {
    for (Entry& e : loadEntries())
        if (e.id == id) return e;
    return std::nullopt;
}

std::string add(const std::string& name, const std::string& exe,
                const std::string& args, const std::string& coverPath) {
    if (exe.empty()) return "";
    std::vector<Entry> entries = loadEntries();

    // Generate a slug id unique among the existing ids (…, -2, -3 on collision).
    std::set<std::string> taken;
    for (const Entry& e : entries) taken.insert(e.id);
    std::string base = slugify(name.empty() ? fs::path(exe).stem().string() : name);
    std::string id = base;
    for (int n = 2; taken.count(id); ++n) id = base + "-" + std::to_string(n);

    Entry e;
    e.id = id;
    e.name = name.empty() ? fs::path(exe).stem().string() : name;
    e.exe = exe;
    e.args = args;
    e.workingDir = fs::path(exe).parent_path().string();   // default cwd = exe folder
    e.coverPath = coverPath;
    entries.push_back(std::move(e));
    saveEntries(entries);
    return id;
}

bool update(const Entry& e) {
    std::vector<Entry> entries = loadEntries();
    for (Entry& cur : entries) {
        if (cur.id != e.id) continue;
        cur.name = e.name;
        cur.exe = e.exe;
        cur.args = e.args;
        // Keep the cwd tracking the exe folder unless the caller set one explicitly.
        cur.workingDir = e.workingDir.empty() ? fs::path(e.exe).parent_path().string()
                                              : e.workingDir;
        cur.coverPath = e.coverPath;
        saveEntries(entries);
        return true;
    }
    return false;
}

bool remove(const std::string& id) {
    std::vector<Entry> entries = loadEntries();
    auto it = std::remove_if(entries.begin(), entries.end(),
                             [&](const Entry& e) { return e.id == id; });
    if (it == entries.end()) return false;
    entries.erase(it, entries.end());
    saveEntries(entries);
    return true;
}

Game toGame(const Entry& e) {
    Game g;
    g.store = Store::Custom;
    g.appid = 0;                 // no numeric id; identity is launchId (the slug)
    g.launchId = e.id;
    g.name = e.name.empty() ? e.id : e.name;
    g.installdir = launchWorkingDir(e);
    // "<name>|<local image path>|<exe path>": the local image overrides everything;
    // the exe path lets store_covers read install-folder breadcrumbs (steam_appid.txt
    // / goggame) + the exe's embedded icon; the name is the Steam-store fallback.
    g.coverHint = g.name + "|" + e.coverPath + "|" + e.exe;
    std::error_code ec;
    g.fullyInstalled = !e.exe.empty() && fs::exists(e.exe, ec);
    return g;
}

std::vector<Game> installedGames() {
    std::vector<Game> out;
    for (const Entry& e : loadEntries()) out.push_back(toGame(e));
    std::sort(out.begin(), out.end(),
              [](const Game& a, const Game& b) { return a.name < b.name; });
    return out;
}

std::vector<std::string> launchArgv(const Entry& e) {
    if (e.exe.empty()) return {};
    std::vector<std::string> argv = {e.exe};
    std::istringstream in(e.args);
    std::string tok;
    while (in >> tok) argv.push_back(tok);
    return argv;
}

std::string launchWorkingDir(const Entry& e) {
    if (!e.workingDir.empty()) return e.workingDir;
    return fs::path(e.exe).parent_path().string();
}

PlayResult launch(const std::string& id, const Notify& notify) {
    auto e = find(id);
    if (!e) return PlayResult::fail("That game is no longer in your library.");
    std::error_code ec;
    if (e->exe.empty() || !fs::exists(e->exe, ec))
        return PlayResult::fail("Can't find the game file for \"" + e->name + "\".");
    if (notify) notify("Launching \"" + e->name + "\"…");
    platform::spawnDetached(launchArgv(*e), launchWorkingDir(*e));
    return PlayResult::success("Launched \"" + e->name + "\".");
}

}  // namespace ss::custom
