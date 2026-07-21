// Custom games — user-authored library entries persisted to custom_games.json.
// Covers the pure builders (slug/argv/cwd/toGame) and the add/find/update/remove
// round-trip against a temp app-data dir.

#include "test_util.h"

#include "../core/appdata.h"
#include "../core/custom_games.h"
#include "../core/model.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ss;

static fs::path tmpData(const char* tag) {
    fs::path d = fs::temp_directory_path() /
                 (std::string("ss_custom_") + tag + "_" +
                  std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(d);
    return d;
}

TEST_CASE(custom_slugify) {
    CHECK_EQ(custom::slugify("My Game!"), std::string("my-game"));
    CHECK_EQ(custom::slugify("  Halo:  Reach  "), std::string("halo-reach"));
    CHECK_EQ(custom::slugify("Portal 2"), std::string("portal-2"));
    CHECK_EQ(custom::slugify("***"), std::string("game"));   // no alnum -> fallback
    CHECK_EQ(custom::slugify(""), std::string("game"));
}

TEST_CASE(custom_launch_argv_and_cwd) {
    custom::Entry e;
    e.exe = "/games/cool/cool.exe";
    e.args = "-windowed  -novid";     // collapses on whitespace
    auto argv = custom::launchArgv(e);
    CHECK_EQ(argv.size(), (size_t)3);
    CHECK_EQ(argv[0], std::string("/games/cool/cool.exe"));
    CHECK_EQ(argv[1], std::string("-windowed"));
    CHECK_EQ(argv[2], std::string("-novid"));
    // cwd defaults to the exe's folder when workingDir is empty.
    CHECK_EQ(custom::launchWorkingDir(e), fs::path("/games/cool/cool.exe").parent_path().string());
    e.workingDir = "/somewhere/else";
    CHECK_EQ(custom::launchWorkingDir(e), std::string("/somewhere/else"));
    // No exe => empty argv (nothing to launch).
    custom::Entry empty;
    CHECK(custom::launchArgv(empty).empty());
}

TEST_CASE(custom_to_game) {
    custom::Entry e;
    e.id = "portal-2";
    e.name = "Portal 2";
    e.exe = "/no/such/portal2.exe";
    e.coverPath = "/art/p2.png";
    Game g = custom::toGame(e);
    CHECK(g.store == Store::Custom);
    CHECK_EQ(g.appid, (int64_t)0);
    CHECK_EQ(g.launchId, std::string("portal-2"));
    CHECK_EQ(g.name, std::string("Portal 2"));
    // "<name>|<local image>|<exe>"
    CHECK_EQ(g.coverHint, std::string("Portal 2|/art/p2.png|/no/such/portal2.exe"));
    CHECK_EQ(g.fullyInstalled, false);           // exe doesn't exist on disk
    CHECK_EQ(std::string(storeName(Store::Custom)), std::string("Custom"));
}

TEST_CASE(custom_add_find_update_remove_round_trip) {
    fs::path data = tmpData("crud");
    CHECK(custom::load().empty());
    CHECK_EQ(custom::add("", ""), std::string(""));      // no exe -> rejected

    std::string id = custom::add("My Game", "/g/mygame/run.exe", "-x");
    CHECK_EQ(id, std::string("my-game"));
    CHECK(fs::exists(data / "custom_games.json"));

    auto e = custom::find(id);
    CHECK(e.has_value());
    CHECK_EQ(e->name, std::string("My Game"));
    CHECK_EQ(e->exe, std::string("/g/mygame/run.exe"));
    CHECK_EQ(e->args, std::string("-x"));
    // workingDir auto-defaulted to the exe folder.
    CHECK_EQ(e->workingDir, fs::path("/g/mygame/run.exe").parent_path().string());

    // A second add with the same name gets a distinct, non-colliding id.
    std::string id2 = custom::add("My Game", "/g/other/run.exe");
    CHECK_EQ(id2, std::string("my-game-2"));
    CHECK_EQ(custom::load().size(), (size_t)2);

    // Update in place.
    custom::Entry upd;
    upd.id = id;
    upd.name = "Renamed";
    upd.exe = "/g/mygame/run.exe";
    upd.args = "-y";
    upd.coverPath = "/art/c.jpg";
    CHECK(custom::update(upd));
    auto e2 = custom::find(id);
    CHECK_EQ(e2->name, std::string("Renamed"));
    CHECK_EQ(e2->args, std::string("-y"));
    CHECK_EQ(e2->coverPath, std::string("/art/c.jpg"));
    CHECK(!custom::update(custom::Entry{"nope", "", "", "", "", ""}));   // unknown id

    // installedGames reflects the store, sorted by name.
    auto games = custom::installedGames();
    CHECK_EQ(games.size(), (size_t)2);
    for (const auto& g : games) CHECK(g.store == Store::Custom);

    // Remove.
    CHECK(custom::remove(id2));
    CHECK(!custom::remove(id2));                 // already gone
    CHECK_EQ(custom::load().size(), (size_t)1);
    CHECK(custom::find(id).has_value());
    CHECK(!custom::find(id2).has_value());

    fs::remove_all(data);
}
