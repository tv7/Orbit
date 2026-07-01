// Epic store — enumerate installed games from synthetic *.item manifests + build
// the launch URI. No real Epic install needed (mirrors smoke_test.py's approach).

#include "test_util.h"

#include "../core/epic_games.h"
#include "../core/model.h"
#include "../core/stores/store.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace ss;

namespace {

fs::path makeManifestDir() {
    fs::path dir = fs::temp_directory_path()
        / ("ss_epic_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    fs::create_directories(dir);
    return dir;
}

void writeItem(const fs::path& dir, const std::string& file, const std::string& json) {
    std::ofstream(dir / file, std::ios::binary) << json;
}

}  // namespace

TEST_CASE(epic_launch_uri_format) {
    CHECK_EQ(epic::launchUri("Fortnite"),
             std::string("com.epicgames.launcher://apps/Fortnite?action=launch&silent=true"));
}

TEST_CASE(epic_scan_reads_base_games_and_filters_dlc) {
    fs::path dir = makeManifestDir();
    // A base game.
    writeItem(dir, "a.item", R"({
        "AppName": "Fortnite",
        "DisplayName": "Fortnite",
        "InstallLocation": "C:\\Games\\Fortnite",
        "AppCategories": ["public", "games", "applications"],
        "bIsIncompleteInstall": false
    })");
    // A second base game, still installing.
    writeItem(dir, "b.item", R"({
        "AppName": "0a2d9f",
        "DisplayName": "Alan Wake 2",
        "InstallLocation": "C:\\Games\\AW2",
        "AppCategories": ["games"],
        "bIsIncompleteInstall": true
    })");
    // A DLC/addon — must be filtered out (addons category + a different MainGame).
    writeItem(dir, "c.item", R"({
        "AppName": "SkinPack",
        "DisplayName": "Fancy Skins",
        "InstallLocation": "C:\\Games\\Fortnite",
        "AppCategories": ["addons"],
        "MainGameAppName": "Fortnite"
    })");
    // Junk / non-.item is ignored.
    writeItem(dir, "notes.txt", "ignore me");

    ss::test::setEnv("SS_EPIC_MANIFESTS", dir.string());

    auto games = epic::installedGames();
    CHECK_EQ(games.size(), static_cast<size_t>(2));
    // Sorted by name: "Alan Wake 2" before "Fortnite".
    CHECK_EQ(games[0].name, std::string("Alan Wake 2"));
    CHECK_EQ(games[0].store == Store::Epic, true);
    CHECK_EQ(games[0].launchId, std::string("0a2d9f"));
    CHECK_EQ(games[0].fullyInstalled, false);       // still installing
    CHECK_EQ(games[1].name, std::string("Fortnite"));
    CHECK_EQ(games[1].launchId, std::string("Fortnite"));
    CHECK_EQ(games[1].fullyInstalled, true);
    CHECK_EQ(games[1].appid, static_cast<int64_t>(0));   // Epic has no numeric appid
}

TEST_CASE(epic_store_scan_and_missing_dir) {
    // A store over a non-existent dir yields no games (Epic not installed).
    ss::test::setEnv("SS_EPIC_MANIFESTS",
        (fs::temp_directory_path() / "ss_epic_does_not_exist_xyz").string());
    auto store = makeEpicStore();
    CHECK_EQ(store->kind() == Store::Epic, true);
    CHECK_EQ(store->scan().empty(), true);
    CHECK_EQ(store->accounts().empty(), true);   // Epic has no account switching
}
