// Settings persistence — mirrors server.py's set_language/_load_settings round-trip.

#include "test_util.h"

#include "../core/appdata.h"
#include "../core/settings.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ss;

TEST_CASE(settings_language_defaults_to_en) {
    fs::path data = fs::temp_directory_path() / ("ss_set_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    CHECK_EQ(settings::language(), std::string("en"));   // no file yet
}

TEST_CASE(settings_language_round_trips) {
    fs::path data = fs::temp_directory_path() / ("ss_set2_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    settings::setLanguage("ar");
    CHECK_EQ(settings::language(), std::string("ar"));
    settings::setLanguage("en");                          // overwrite persists
    CHECK_EQ(settings::language(), std::string("en"));
    CHECK(fs::exists(data / "settings.json"));
}
