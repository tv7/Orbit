// Per-store cover resolution: base64 + Epic catcache URL selection (pure), the
// Xbox local-logo read, and the GOG API fetch + disk-cache via a stub fetcher.

#include "test_util.h"

#include "../core/appdata.h"
#include "../core/http.h"
#include "../core/store_covers.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace ss;

namespace {
fs::path freshDir(const char* tag) {
    fs::path d = fs::temp_directory_path()
        / (std::string("ss_scov_") + tag + "_" + std::to_string(ss::test::procId()));
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}
}  // namespace

TEST_CASE(base64_decode_roundtrip) {
    CHECK_EQ(store_covers::base64Decode("aGVsbG8=").value_or(""), std::string("hello"));
    CHECK_EQ(store_covers::base64Decode("aGVsbG8h").value_or(""), std::string("hello!"));
    CHECK_EQ(store_covers::base64Decode("bad~chars").has_value(), false);
}

TEST_CASE(epic_catalog_prefers_tall_box) {
    std::string catalog = R"([
      {"id":"other","keyImages":[{"type":"DieselGameBoxTall","url":"https://x/no.jpg"}]},
      {"id":"cat123","keyImages":[
          {"type":"Thumbnail","url":"https://x/thumb.jpg"},
          {"type":"DieselGameBox","url":"https://x/wide.jpg"},
          {"type":"DieselGameBoxTall","url":"https://x/tall.jpg"}]}
    ])";
    CHECK_EQ(store_covers::epicCoverUrlFromCatalog(catalog, "cat123"),
             std::string("https://x/tall.jpg"));
    CHECK_EQ(store_covers::epicCoverUrlFromCatalog(catalog, "missing"), std::string(""));
}

TEST_CASE(xbox_cover_reads_local_logo) {
    fs::path dir = freshDir("xbox");
    std::ofstream(dir / "logo.png", std::ios::binary) << "\x89PNG-bytes";
    auto got = store_covers::coverBytes(Store::Xbox, "PFN!Game",
                                        (dir / "logo.png").string(), 42, false);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("\x89PNG-bytes"));
    // No hint -> no art (and never a network call).
    CHECK_EQ(store_covers::coverBytes(Store::Xbox, "PFN!Game", "", 42, false).has_value(),
             false);
}

TEST_CASE(custom_cover_prefers_local_then_steam_search) {
    fs::path data = freshDir("custom");
    appdata::setDir(data);
    std::ofstream(data / "mine.png", std::ios::binary) << "\x89PNG-mine";

    // coverHint is "<name>|<local image>|<exe>".
    // 1. A user-picked local image wins outright (no network even offered).
    auto local = store_covers::coverBytes(Store::Custom, "my-game",
        std::string("My Game|") + (data / "mine.png").string() + "|", 7001, false);
    CHECK(local.has_value());
    CHECK_EQ(*local, std::string("\x89PNG-mine"));

    // 2. No local image / breadcrumb -> match the name on Steam's store
    //    (storesearch -> appid -> Steam cover), then disk-cache the hit.
    int fetches = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++fetches;
        if (url.find("store.steampowered.com/api/storesearch") != std::string::npos)
            return std::string(R"({"total":1,"items":[{"id":570,"name":"Dota 2"}]})");
        if (url.find("/apps/570/") != std::string::npos) return std::string("STEAMART");
        return std::nullopt;
    });
    auto autoArt = store_covers::coverBytes(Store::Custom, "dota", "Dota 2||", 7002);
    CHECK_EQ(autoArt.value_or(""), std::string("STEAMART"));
    int after = fetches;
    auto again = store_covers::coverBytes(Store::Custom, "dota", "Dota 2||", 7002);
    CHECK_EQ(again.value_or(""), std::string("STEAMART"));
    CHECK_EQ(fetches, after);                        // served from the custom disk cache
    http::setFetcher({});

    // 3. No image, no match, offline -> nothing (plain gradient tile; the exe-icon
    //    fallback is Windows-only so it's nullopt here).
    CHECK_EQ(store_covers::coverBytes(Store::Custom, "x", "Nope||", 7003, false).has_value(),
             false);
    fs::remove_all(data);
}

TEST_CASE(custom_cover_uses_install_folder_breadcrumbs) {
    fs::path data = freshDir("custombread");
    appdata::setDir(data);
    // A game dir with the original store's leftovers + a (fake) exe next to them.
    fs::path gameDir = data / "GameDir";
    fs::create_directories(gameDir);
    std::ofstream(gameDir / "steam_appid.txt", std::ios::binary) << "620\r\n";
    fs::path exe = gameDir / "game.exe";
    std::ofstream(exe, std::ios::binary) << "MZ";
    std::string hint = std::string("Some Game||") + exe.string();

    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        // steam_appid.txt => appid 620 => Steam cover via the flat CDN. The name
        // search must NOT be reached (the breadcrumb resolves first).
        if (url.find("storesearch") != std::string::npos) return std::string("SHOULD-NOT-HAPPEN");
        if (url.find("/apps/620/") != std::string::npos) return std::string("PORTAL2ART");
        return std::nullopt;
    });
    auto got = store_covers::coverBytes(Store::Custom, "some-game", hint, 8001);
    CHECK_EQ(got.value_or(""), std::string("PORTAL2ART"));
    http::setFetcher({});

    // GOG breadcrumb: goggame-<id>.info => product id => GOG box art.
    fs::path gogDir = data / "GogDir";
    fs::create_directories(gogDir);
    std::ofstream(gogDir / "goggame-1207664663.info", std::ios::binary)
        << R"({"gameId":"1207664663","name":"X"})";
    fs::path gexe = gogDir / "g.exe";
    std::ofstream(gexe, std::ios::binary) << "MZ";
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        if (url == "https://api.gog.com/v2/games/1207664663")
            return std::string(R"({"_links":{"boxArtImage":{"href":"//images.gog.com/b.jpg"}}})");
        if (url == "https://images.gog.com/b.jpg") return std::string("GOGBOX");
        return std::nullopt;
    });
    auto gog = store_covers::coverBytes(Store::Custom, "x",
        std::string("X||") + gexe.string(), 8002);
    CHECK_EQ(gog.value_or(""), std::string("GOGBOX"));
    http::setFetcher({});
    fs::remove_all(data);
}

TEST_CASE(gog_cover_fetches_boxart_and_disk_caches) {
    fs::path data = freshDir("gog");
    appdata::setDir(data);
    int fetches = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++fetches;
        if (url == "https://api.gog.com/v2/games/1207664663")
            return std::string(R"({"_links":{"boxArtImage":{"href":"//images.gog.com/box.jpg"}}})");
        if (url == "https://images.gog.com/box.jpg") return std::string("GOGIMG");
        return std::nullopt;
    });
    auto got = store_covers::coverBytes(Store::Gog, "1207664663", "", 99);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("GOGIMG"));
    int after = fetches;
    // Second call must come from the disk cache — no new network round-trips.
    auto again = store_covers::coverBytes(Store::Gog, "1207664663", "", 99);
    CHECK_EQ(again.value_or(""), std::string("GOGIMG"));
    CHECK_EQ(fetches, after);
    http::setFetcher({});
}

TEST_CASE(epic_hero_prefers_wide_box) {
    std::string catalog = R"([
      {"id":"cat123","keyImages":[
          {"type":"Thumbnail","url":"https://x/thumb.jpg"},
          {"type":"DieselGameBox","url":"https://x/wide.jpg"},
          {"type":"DieselGameBoxTall","url":"https://x/tall.jpg"}]},
      {"id":"tallonly","keyImages":[
          {"type":"DieselGameBoxTall","url":"https://x/tall2.jpg"}]}
    ])";
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "cat123"),
             std::string("https://x/wide.jpg"));
    // Falls back to the tall box rather than nothing.
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "tallonly"),
             std::string("https://x/tall2.jpg"));
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "missing"), std::string(""));
}

TEST_CASE(xbox_hero_uses_titled_hero_art_and_caches) {
    fs::path data = freshDir("xheros");
    appdata::setDir(data);
    int fetches = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++fetches;
        if (url.find("displaycatalog.mp.microsoft.com") != std::string::npos)
            return std::string(R"({"Products":[{"LocalizedProperties":[{"Images":[
                {"ImagePurpose":"Poster","Uri":"//cdn/poster.jpg"},
                {"ImagePurpose":"TitledHeroArt","Uri":"//cdn/hero.jpg"}]}]}]})");
        if (url.rfind("https://cdn/hero.jpg", 0) == 0) return std::string("XHERO");
        return std::nullopt;
    });
    // coverHint = "<StoreId>|<logo path>"; the hero must pick TitledHeroArt, not Poster.
    auto got = store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4242);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("XHERO"));
    CHECK(fs::exists(data / "covers" / "4242_hero.jpg"));
    int after = fetches;
    auto again = store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4242);
    CHECK_EQ(again.value_or(""), std::string("XHERO"));
    CHECK_EQ(fetches, after);                       // disk cache, no re-fetch
    // Offline + uncached -> nothing (no local hero source for Xbox).
    CHECK_EQ(store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4243,
                                     false).has_value(), false);
    http::setFetcher({});
}

TEST_CASE(gog_hero_uses_background_image) {
    fs::path data = freshDir("gheros");
    appdata::setDir(data);
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        if (url == "https://api.gog.com/v2/games/1207664663")
            return std::string(R"({"_links":{
                "boxArtImage":{"href":"//images.gog.com/box.jpg"},
                "backgroundImage":{"href":"//images.gog.com/bg.jpg"}}})");
        if (url == "https://images.gog.com/bg.jpg") return std::string("GOGBG");
        return std::nullopt;
    });
    auto got = store_covers::heroBytes(Store::Gog, "1207664663", "", 77);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("GOGBG"));           // background, not the box art
    http::setFetcher({});
}
