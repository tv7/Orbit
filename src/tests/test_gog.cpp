// GOG store — the registry enumeration is Windows-only (empty here), so we unit-test
// the pure builders (Game mapping, Galaxy vs direct launch argv, working dir) from
// synthetic registry entries, plus the empty-on-POSIX enumeration path.

#include "test_util.h"

#include "../core/gog_games.h"
#include "../core/model.h"
#include "../core/stores/store.h"

using namespace ss;

namespace {
gog::Entry sampleEntry() {
    gog::Entry e;
    e.id = "1207658930";
    e.name = "The Witcher 3: Wild Hunt";
    e.path = "D:\\GOG Games\\The Witcher 3";
    e.launchCommand = "D:\\GOG Games\\The Witcher 3\\bin\\x64\\witcher3.exe";
    e.launchParam = "";
    e.workingDir = "D:\\GOG Games\\The Witcher 3\\bin\\x64";
    e.exe = "D:\\GOG Games\\The Witcher 3\\bin\\x64\\witcher3.exe";
    return e;
}
}  // namespace

TEST_CASE(gog_to_game_maps_fields) {
    Game g = gog::toGame(sampleEntry());
    CHECK_EQ(g.store == Store::Gog, true);
    CHECK_EQ(g.appid, static_cast<int64_t>(0));
    CHECK_EQ(g.launchId, std::string("1207658930"));
    CHECK_EQ(g.name, std::string("The Witcher 3: Wild Hunt"));
    CHECK_EQ(g.installdir, std::string("D:\\GOG Games\\The Witcher 3"));
    CHECK_EQ(g.fullyInstalled, true);
}

TEST_CASE(gog_galaxy_runGame_argv) {
    auto argv = gog::galaxyRunGameArgv("C:\\GOG Galaxy\\GalaxyClient.exe", sampleEntry());
    CHECK_EQ(argv.size(), static_cast<size_t>(4));
    CHECK_EQ(argv[0], std::string("C:\\GOG Galaxy\\GalaxyClient.exe"));
    CHECK_EQ(argv[1], std::string("/command=runGame"));
    CHECK_EQ(argv[2], std::string("/gameId=1207658930"));
    CHECK_EQ(argv[3], std::string("/path=D:\\GOG Games\\The Witcher 3"));
}

TEST_CASE(gog_direct_launch_argv_prefers_launchCommand) {
    auto argv = gog::directLaunchArgv(sampleEntry());
    CHECK_EQ(argv.size(), static_cast<size_t>(1));
    CHECK_EQ(argv[0], std::string("D:\\GOG Games\\The Witcher 3\\bin\\x64\\witcher3.exe"));
    CHECK_EQ(gog::launchWorkingDir(sampleEntry()),
             std::string("D:\\GOG Games\\The Witcher 3\\bin\\x64"));
}

TEST_CASE(gog_direct_launch_argv_falls_back_to_exe_and_splits_params) {
    gog::Entry e;
    e.id = "42";
    e.name = "Some Game";
    e.path = "C:\\Games\\Some";
    e.exe = "C:\\Games\\Some\\game.exe";       // no launchCommand → use exe
    e.launchParam = "-windowed -skipintro";
    auto argv = gog::directLaunchArgv(e);
    CHECK_EQ(argv.size(), static_cast<size_t>(3));
    CHECK_EQ(argv[0], std::string("C:\\Games\\Some\\game.exe"));
    CHECK_EQ(argv[1], std::string("-windowed"));
    CHECK_EQ(argv[2], std::string("-skipintro"));
    // workingDir empty → falls back to install path.
    CHECK_EQ(gog::launchWorkingDir(e), std::string("C:\\Games\\Some"));
}

TEST_CASE(gog_direct_launch_argv_empty_when_no_exe) {
    gog::Entry e; e.id = "7"; e.name = "No Exe";
    CHECK_EQ(gog::directLaunchArgv(e).empty(), true);
}

TEST_CASE(gog_store_enumeration_empty_without_registry) {
    // No Windows registry here → no GOG games, empty accounts, right kind.
    auto store = makeGogStore();
    CHECK_EQ(store->kind() == Store::Gog, true);
    CHECK_EQ(store->scan().empty(), true);
    CHECK_EQ(store->accounts().empty(), true);
    CHECK_EQ(gog::entry("does-not-exist").has_value(), false);
}
